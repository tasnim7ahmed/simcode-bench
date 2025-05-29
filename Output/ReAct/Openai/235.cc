#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lorawan-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LorawanSensorSimulation");

class UdpSinkApp : public Application
{
public:
  UdpSinkApp () {}
  virtual ~UdpSinkApp () {}

  void Setup (Address address, uint16_t port)
  {
    m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), port);
    m_socket->Bind (local);
    m_socket->SetRecvCallback (MakeCallback (&UdpSinkApp::HandleRead, this));
  }

private:
  virtual void StartApplication (void)
  {
    // Nothing to do
  }
  virtual void StopApplication (void)
  {
    if (m_socket)
      m_socket->Close ();
  }

  void HandleRead (Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom (from)))
      {
        uint8_t buffer[1024];
        size_t size = packet->CopyData (buffer, 1024);
        std::string data(reinterpret_cast<const char*>(buffer), size);
        NS_LOG_UNCOND ("Sink received packet of size " << size << " bytes: " << data);
      }
  }

  Ptr<Socket> m_socket;
};

class PeriodicUdpSender : public Application
{
public:
  PeriodicUdpSender () {}
  virtual ~PeriodicUdpSender () {}

  void Setup (Address address, uint16_t port, Time interval)
  {
    m_peerAddress = address;
    m_peerPort = port;
    m_interval = interval;
  }

private:
  virtual void StartApplication (void)
  {
    m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
    m_socket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom (m_peerAddress), m_peerPort));
    SendPacket ();
  }
  virtual void StopApplication (void)
  {
    if (m_socket)
      m_socket->Close ();
  }
  void SendPacket ()
  {
    std::string msg = "Sensor data payload";
    Ptr<Packet> packet = Create<Packet> ((uint8_t*) msg.data (), msg.size ());
    m_socket->Send (packet);

    Simulator::Schedule (m_interval, &PeriodicUdpSender::SendPacket, this);
  }

  Ptr<Socket> m_socket;
  Address m_peerAddress;
  uint16_t m_peerPort;
  Time m_interval;
};

int main (int argc, char *argv[])
{
  LogComponentEnable ("LorawanSensorSimulation", LOG_LEVEL_INFO);

  // --- 1. Create nodes: Sensor (end device), Gateway, Sink
  NodeContainer endDevices;
  endDevices.Create (1);

  NodeContainer gateways;
  gateways.Create (1);

  NodeContainer sinks;
  sinks.Create (1);

  // --- 2. The mobility model: fixed positions
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));   // Sensor
  positionAlloc->Add (Vector (50.0, 0.0, 0.0));  // Gateway
  positionAlloc->Add (Vector (100.0, 0.0, 0.0)); // Sink
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (endDevices);
  mobility.Install (gateways);
  mobility.Install (sinks);

  // --- 3. Install LoRaWAN
  // a) Install the LoRa PHY and MAC
  LorawanHelper lorawanHelper;
  Ptr<UniformRandomVariable> random = CreateObject<UniformRandomVariable> ();

  // b) End device network interface
  NetDeviceContainer endDeviceDevs = lorawanHelper.Install (endDevices, gateways);
  // Gateway device (GWNetDevice)
  NetDeviceContainer gatewayDevs = lorawanHelper.InstallGateway (gateways);

  // c) Connect packet-deliverable LoRaWAN channel between ED and GW
  lorawanHelper.EnablePacketDeliveryTrace ();

  // --- 4. Install Internet stack
  InternetStackHelper stack;
  stack.Install (gateways);
  stack.Install (sinks);

  // GW & Sink are connected via wired link (for LoRaWAN packet forwarding)
  NodeContainer gatewaySink;
  gatewaySink.Add (gateways.Get (0));
  gatewaySink.Add (sinks.Get (0));

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
  NetDeviceContainer p2pDevs = p2p.Install (gatewaySink);

  // --- 5. Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (p2pDevs);

  Ipv4Address sinkAddress = interfaces.GetAddress (1);

  // --- 6. Configure GW UDP forwarding port
  uint16_t udpPort = 8000;

  // Gateway forwards LoRaWAN packets to sink via UDP
  class SimpleGatewayForwarder : public Application
  {
  public:
    void Setup (Ptr<NetDevice> lorawanDev, Address sinkAddress, uint16_t udpPort)
    {
      m_lorawanDev = lorawanDev;
      m_sinkAddr = sinkAddress;
      m_udpPort = udpPort;
    }
    virtual void StartApplication ()
    {
      m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
      m_socket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom (m_sinkAddr), m_udpPort));
      m_lorawanDev->TraceConnectWithoutContext (
        "ReceivedPacket", MakeCallback (&SimpleGatewayForwarder::LoRaRxCallback, this));
    }
    virtual void StopApplication ()
    {
      if (m_socket)
        m_socket->Close ();
    }
    void LoRaRxCallback (Ptr<const Packet> packet)
    {
      Ptr<Packet> copy = packet->Copy ();
      m_socket->Send (copy);
    }
  private:
    Ptr<NetDevice> m_lorawanDev;
    Address m_sinkAddr;
    uint16_t m_udpPort;
    Ptr<Socket> m_socket;
  };

  // --- 7. Install Apps

  // a) Sink: logs incoming UDP data
  Ptr<UdpSinkApp> sinkApp = CreateObject<UdpSinkApp> ();
  sinkApp->Setup (sinkAddress, udpPort);
  sinks.Get (0)->AddApplication (sinkApp);
  sinkApp->SetStartTime (Seconds (0));
  sinkApp->SetStopTime (Seconds (60));

  // b) GW: Forwards LoRaWAN pkt to Sink over UDP
  Ptr<SimpleGatewayForwarder> gwApp = CreateObject<SimpleGatewayForwarder> ();
  gwApp->Setup (gatewayDevs.Get (0), sinkAddress, udpPort);
  gateways.Get (0)->AddApplication (gwApp);
  gwApp->SetStartTime (Seconds (0));
  gwApp->SetStopTime (Seconds (60));

  // c) Sensor node: Periodically send LoRaWAN UDP packets to GW
  class LorawanUdpApp : public Application
  {
  public:
    void Setup (Ptr<NetDevice> lorawanDev, Address dst, uint16_t port, Time interval)
    {
      m_dev = lorawanDev;
      m_dst = dst;
      m_port = port;
      m_interval = interval;
    }
    virtual void StartApplication ()
    {
      Send ();
    }
    void Send ()
    {
      std::string msg = "Sensor data payload";
      Ptr<Packet> packet = Create<Packet> ((uint8_t*) msg.data (), msg.size ());
      // LoRaWAN: send to GW MAC address 0 (only 1 gateway)
      m_dev->Send (packet, Mac16Address ("0000"), 0);
      Simulator::Schedule (m_interval, &LorawanUdpApp::Send, this);
    }
  private:
    Ptr<NetDevice> m_dev;
    Address m_dst;
    uint16_t m_port;
    Time m_interval;
  };

  Ptr<LorawanUdpApp> senderApp = CreateObject<LorawanUdpApp> ();
  senderApp->Setup (endDeviceDevs.Get (0), sinkAddress, udpPort, Seconds (5.0));
  endDevices.Get (0)->AddApplication (senderApp);
  senderApp->SetStartTime (Seconds (1));
  senderApp->SetStopTime (Seconds (60));

  Simulator::Stop (Seconds (60.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}