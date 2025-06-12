#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpBulkTransferSimulation");

class TcpBulkSender : public Application
{
public:
  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("TcpBulkSender")
      .SetParent<Application> ()
      .SetGroupName("Applications")
      .AddConstructor<TcpBulkSender> ()
      .AddAttribute ("Remote", "The address of the destination",
                     AddressValue (),
                     MakeAddressAccessor (&TcpBulkSender::m_peer),
                     MakeAddressChecker ())
      .AddAttribute ("PacketSize", "Size of packets to send in bytes",
                     UintegerValue(1024),
                     MakeUintegerAccessor(&TcpBulkSender::m_packetSize),
                     MakeUintegerChecker<uint32_t>(1))
      .AddAttribute ("DataRate", "The data rate at which to send traffic",
                     DataRateValue(DataRate("500kbps")),
                     MakeDataRateAccessor(&TcpBulkSender::m_dataRate),
                     MakeDataRateChecker())
      ;
    return tid;
  }

  TcpBulkSender ();
  virtual ~TcpBulkSender ();

protected:
  void DoInitialize (void) override;
  void DoDispose (void) override;

private:
  void StartApplication (void) override;
  void StopApplication (void) override;
  void SendPacket (void);

  Address m_peer;
  uint32_t m_packetSize;
  DataRate m_dataRate;
  EventId m_sendEvent;
  bool m_running;
  Ptr<Socket> m_socket;
};

TcpBulkSender::TcpBulkSender ()
  : m_peer (),
    m_packetSize (1024),
    m_dataRate ("500kbps"),
    m_running (false),
    m_socket (nullptr)
{
}

TcpBulkSender::~TcpBulkSender ()
{
  m_socket = nullptr;
}

void
TcpBulkSender::DoInitialize (void)
{
  Application::DoInitialize ();
}

void
TcpBulkSender::DoDispose (void)
{
  Application::DoDispose ();
}

void
TcpBulkSender::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  if (!m_socket)
    {
      TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);

      if (m_socket->Bind () == -1)
        {
          NS_FATAL_ERROR ("Failed to bind socket");
        }
    }

  m_socket->Connect (m_peer);
  m_running = true;
  SendPacket ();
}

void
TcpBulkSender::StopApplication (void)
{
  NS_LOG_FUNCTION (this);
  m_running = false;

  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
TcpBulkSender::SendPacket (void)
{
  NS_LOG_FUNCTION (this);

  if (m_running)
    {
      Ptr<Packet> packet = Create<Packet> (m_packetSize);
      int sent = m_socket->Send (packet);
      if (sent < 0)
        {
          NS_LOG_WARN ("Error sending packet: " << sent);
        }
      else
        {
          NS_LOG_INFO ("Sent packet of size " << sent << " bytes");
        }

      Time interPacketInterval = Seconds ((double)m_packetSize * 8 / m_dataRate.GetBitRate ());
      m_sendEvent = Simulator::Schedule (interPacketInterval, &TcpBulkSender::SendPacket, this);
    }
}

static void
OnTx (Ptr<const Packet> packet)
{
  static uint64_t totalBytesTransmitted = 0;
  static Time lastLogTime = Seconds (0.0);
  double throughputInterval = 1.0; // seconds

  totalBytesTransmitted += packet->GetSize ();

  Time now = Simulator::Now ();
  if ((now - lastLogTime).GetSeconds () >= throughputInterval)
    {
      double intervalSec = (now - lastLogTime).GetSeconds ();
      double throughput = (totalBytesTransmitted * 8.0) / (intervalSec * 1000.0); // kbps
      NS_LOG_UNCOND ("[" << now.GetSeconds () << "s] Throughput: " << throughput << " kbps, Total Bytes: " << totalBytesTransmitted << " bytes");
      lastLogTime = now;
    }
}

int
main (int argc, char *argv[])
{
  NS_LOG_INFO ("Creating simulation.");

  std::string rate = "5Mbps";
  std::string delay = "10ms";

  CommandLine cmd (__FILE__);
  cmd.AddValue ("rate", "Data rate for the point-to-point link", rate);
  cmd.AddValue ("delay", "Propagation delay for the point-to-point link", delay);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue (rate));
  pointToPoint.SetChannelAttribute ("Delay", StringValue (delay));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), sinkPort));

  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get (1));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (10.0));

  TcpBulkSender sender;
  sender.SetAttribute ("Remote", AddressValue (sinkAddress));
  sender.SetAttribute ("PacketSize", UintegerValue (1024));
  sender.SetAttribute ("DataRate", DataRateValue (DataRate ("2Mbps")));

  ApplicationContainer apps;
  apps.Add (DynamicCast<Application, TcpBulkSender>(&sender));
  apps.Install (nodes.Get (0));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (9.0));

  Config::ConnectWithoutContext ("/NodeList/*/ApplicationList/*/$TcpBulkSender/Socket/Tx", MakeCallback (&OnTx));

  pointToPoint.EnablePcapAll ("tcp-bulk-transfer");

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Run ();
  Simulator::Destroy ();

  NS_LOG_INFO ("Simulation completed.");
  return 0;
}