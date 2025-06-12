#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/6lowpan-net-device.h"
#include "ns3/lr-wpan-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("IoT6LoWPANSimulation");

class IoTDeviceApplication : public Application
{
public:
  static TypeId GetTypeId();
  IoTDeviceApplication();
  virtual ~IoTDeviceApplication();

protected:
  virtual void StartApplication();
  virtual void StopApplication();

private:
  void SendPacket();
  Ptr<Socket> m_socket;
  Address m_serverAddress;
  uint32_t m_packetSize;
  Time m_interval;
  EventId m_sendEvent;
};

TypeId IoTDeviceApplication::GetTypeId()
{
  static TypeId tid = TypeId("ns3::IoTDeviceApplication")
                          .SetParent<Application>()
                          .AddConstructor<IoTDeviceApplication>()
                          .AddAttribute("ServerAddress",
                                        "The address of the server to send packets to.",
                                        AddressValue(),
                                        MakeAddressAccessor(&IoTDeviceApplication::m_serverAddress),
                                        MakeAddressChecker())
                          .AddAttribute("PacketSize",
                                        "The size of each packet sent.",
                                        UintegerValue(128),
                                        MakeUintegerAccessor(&IoTDeviceApplication::m_packetSize),
                                        MakeUintegerChecker<uint32_t>())
                          .AddAttribute("Interval",
                                        "The time between packet sends.",
                                        TimeValue(Seconds(5)),
                                        MakeTimeAccessor(&IoTDeviceApplication::m_interval),
                                        MakeTimeChecker());
  return tid;
}

IoTDeviceApplication::IoTDeviceApplication()
    : m_socket(nullptr),
      m_packetSize(128),
      m_interval(Seconds(5))
{
}

IoTDeviceApplication::~IoTDeviceApplication()
{
  m_socket = nullptr;
}

void IoTDeviceApplication::StartApplication()
{
  m_socket = Socket::CreateSocket(GetNode(), TypeId::LookupByName("ns3::UdpSocketFactory"));
  m_socket->Connect(m_serverAddress);

  SendPacket();
}

void IoTDeviceApplication::StopApplication()
{
  if (m_sendEvent.IsRunning())
  {
    Simulator::Cancel(m_sendEvent);
  }

  if (m_socket)
  {
    m_socket->Close();
  }
}

void IoTDeviceApplication::SendPacket()
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);

  NS_LOG_INFO("Sent packet with size " << m_packetSize << " to server at " << m_serverAddress);

  m_sendEvent = Simulator::Schedule(m_interval, &IoTDeviceApplication::SendPacket, this);
}

int main(int argc, char *argv[])
{
  LogComponentEnable("IoTDeviceApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  uint32_t numDevices = 5;
  Time simulationTime = Seconds(60.0);

  NodeContainer devices;
  devices.Create(numDevices);

  NodeContainer server;
  server.Create(1);

  // Setup LR-WPAN (IEEE 802.15.4) and mobility
  LrWpanHelper lrWpan;
  NetDeviceContainer deviceDevices;
  lrWpan.SetChannelParameter("ChannelNumber", UintegerValue(11));
  for (uint32_t i = 0; i < numDevices; ++i)
  {
    deviceDevices.Add(lrWpan.Install(devices.Get(i)));
  }

  // Install internet stack with IPv6 and 6LoWPAN
  InternetStackHelper internetv6;
  internetv6.SetIpv4StackEnabled(false);
  internetv6.SetIpv6StackEnabled(true);
  internetv6.Install(devices);
  internetv6.Install(server);

  SixLowPanHelper sixlowpan;
  NetDeviceContainer sixlowpanDevices;
  for (uint32_t i = 0; i < numDevices; ++i)
  {
    sixlowpanDevices.Add(sixlowpan.Install(deviceDevices.Get(i)));
  }

  // Assign IPv6 addresses
  Ipv6AddressHelper address;
  address.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer deviceInterfaces;
  deviceInterfaces = address.Assign(sixlowpanDevices);
  address.NewNetwork();
  Ipv6InterfaceContainer serverInterface;
  serverInterface = address.Assign(server.Get(0)->GetDevice(0));

  // Set up routing so devices can reach the server
  Ipv6StaticRoutingHelper routingHelper;
  for (uint32_t i = 0; i < numDevices; ++i)
  {
    Ptr<Ipv6StaticRouting> deviceRouting = routingHelper.GetStaticRouting(devices.Get(i)->GetObject<Ipv6>());
    deviceRouting->AddHostRouteTo(serverInterface.GetAddress(0), 1); // Assuming interface index 1
  }

  // Install UDP server on the server node
  UdpServerHelper serverAppHelper(9); // port 9
  ApplicationContainer serverApp = serverAppHelper.Install(server);
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(simulationTime);

  // Install custom IoT client applications on all devices
  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < numDevices; ++i)
  {
    Ptr<IoTDeviceApplication> app = CreateObject<IoTDeviceApplication>();
    app->SetAttribute("ServerAddress", AddressValue(InetSocketAddress(serverInterface.GetAddress(0), 9)));
    app->SetAttribute("PacketSize", UintegerValue(64 + i * 10)); // Varying sizes
    app->SetAttribute("Interval", TimeValue(Seconds(10)));       // Every 10 seconds

    devices.Get(i)->AddApplication(app);
    app->SetStartTime(Seconds(2.0));
    app->SetStopTime(simulationTime);
  }

  // Mobility setup (optional)
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(5.0),
                                "GridWidth", UintegerValue(5),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(devices);
  mobility.Install(server);

  // Enable PCAP tracing
  lrWpan.EnablePcapAll("iot-6lowpan-sim", false);

  // Animate the simulation
  AnimationInterface anim("iot-6lowpan.xml");
  anim.EnablePacketMetadata(); // Optional
  anim.EnableIpv4L3ProtocolCounters(MilliSeconds(100));
  anim.EnableIpv6L3ProtocolCounters(MilliSeconds(100));

  Simulator::Stop(simulationTime);
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}