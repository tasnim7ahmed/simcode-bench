#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpClientServerSimulation");

class TcpClient : public Application
{
public:
  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("TcpClient")
      .SetParent<Application> ()
      .AddConstructor<TcpClient> ()
      .AddAttribute ("RemoteAddress", "The address of the destination",
                     AddressValue (),
                     MakeAddressAccessor (&TcpClient::m_peerAddress),
                     MakeAddressChecker ())
      .AddAttribute ("PacketSize", "Size of packets to send in bytes.",
                     UintegerValue(512),
                     MakeUintegerAccessor(&TcpClient::m_packetSize),
                     MakeUintegerChecker<uint32_t>())
      .AddAttribute ("Interval", "Time between sending packets.",
                     TimeValue (Seconds (0.1)),
                     MakeTimeAccessor (&TcpClient::m_interval),
                     MakeTimeChecker ());
    return tid;
  }

  TcpClient () {}
  virtual ~TcpClient () {}

private:
  virtual void StartApplication (void)
  {
    m_socket = Socket::CreateSocket (GetNode (), TcpSocketFactory::GetTypeId ());
    m_socket->Connect (InetSocketAddress::ConvertFrom(m_peerAddress));
    m_sendEvent = Simulator::Schedule (Seconds (0.0), &TcpClient::SendPacket, this);
    m_count = 0;
  }

  virtual void StopApplication (void)
  {
    if (m_socket)
      {
        m_socket->Close ();
        m_socket = nullptr;
      }

    if (!m_sendEvent.IsExpired ())
      {
        Simulator::Cancel (m_sendEvent);
      }
  }

  void SendPacket ()
  {
    if (m_count < 100)
      {
        Ptr<Packet> packet = Create<Packet> (m_packetSize);
        m_socket->Send (packet);
        m_count++;
        m_sendEvent = Simulator::Schedule (m_interval, &TcpClient::SendPacket, this);
      }
  }

  Address m_peerAddress;
  uint32_t m_packetSize;
  Time m_interval;
  uint32_t m_count;
  EventId m_sendEvent;
  Ptr<Socket> m_socket;
};

int main (int argc, char *argv[])
{
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpSocketBase::GetTypeId ()));

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t port = 9;

  // Server application
  PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sink.Install (nodes.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (10.0));

  // Client application
  TcpClientHelper client ("ns3::TcpSocketFactory", InetSocketAddress (interfaces.GetAddress (1), port));
  client.SetAttribute ("PacketSize", UintegerValue (512));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  ApplicationContainer clientApp = client.Install (nodes.Get (0));
  clientApp.Start (Seconds (0.0));
  clientApp.Stop (Seconds (10.0));

  // Flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1000 / 1000 << " Mbps\n";
    }

  Simulator::Destroy ();

  return 0;
}