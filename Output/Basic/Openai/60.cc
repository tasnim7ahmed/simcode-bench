#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"
#include "ns3/traffic-control-module.h"
#include <fstream>

using namespace ns3;

void
CwndTracer(uint32_t oldCwnd, uint32_t newCwnd)
{
  static std::ofstream cwndStream("cwnd-trace.txt", std::ios_base::out | std::ios_base::app);
  cwndStream << Simulator::Now().GetSeconds() << " " << newCwnd << std::endl;
}

void
RxTracer(Ptr<const Packet> packet, const Address &addr)
{
  static std::ofstream rxStream("rx-trace.txt", std::ios_base::out | std::ios_base::app);
  rxStream << Simulator::Now().GetSeconds() << " " << packet->GetSize() << std::endl;
}

int
main(int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(3);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("10ms"));
  p2p.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("100p"));

  NetDeviceContainer dev01 = p2p.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer dev12 = p2p.Install(nodes.Get(1), nodes.Get(2));

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer if01 = ipv4.Assign(dev01);
  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer if12 = ipv4.Assign(dev12);

  uint16_t sinkPort = 8080;
  Address sinkAddress(InetSocketAddress(if12.GetAddress(1), sinkPort));
  Address anyAddress(InetSocketAddress(Ipv4Address::GetAny(), sinkPort));

  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", anyAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(2));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(30.0));

  Ptr<Socket> srcSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
  srcSocket->SetAttribute("SndBufSize", UintegerValue(2000000));
  srcSocket->SetAttribute("RcvBufSize", UintegerValue(2000000));

  Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp.Get(0));
  Simulator::Schedule(Seconds(1.0), &Socket::Bind, srcSocket, InetSocketAddress(Ipv4Address::GetAny(), 0));

  uint32_t totalBytes = 2000000;
  uint32_t segmentSize = 1448;
  srcSocket->Connect(InetSocketAddress(if12.GetAddress(1), sinkPort));

  srcSocket->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&CwndTracer));
  sink->TraceConnectWithoutContext("Rx", MakeCallback(&RxTracer));

  class SenderApp : public Application
  {
  public:
    SenderApp() : m_socket(0), m_sent(0), m_total(0), m_pktSize(0) {}
    void Setup(Ptr<Socket> socket, Address address, uint32_t total, uint32_t pktSize)
    {
      m_socket = socket;
      m_peer = address;
      m_total = total;
      m_pktSize = pktSize;
      m_sendEvent = EventId();
      m_sent = 0;
    }

    virtual void StartApplication()
    {
      m_socket->Bind();
      m_socket->Connect(m_peer);
      SendPacket();
    }

    virtual void StopApplication()
    {
      if (m_socket)
      {
        m_socket->Close();
      }
      Simulator::Cancel(m_sendEvent);
    }

    void SendPacket()
    {
      while (m_sent < m_total && m_socket->GetTxAvailable() >= m_pktSize)
      {
        uint32_t left = m_total - m_sent;
        uint32_t sendSize = std::min(left, m_pktSize);
        Ptr<Packet> packet = Create<Packet>(sendSize);
        m_socket->Send(packet);
        m_sent += sendSize;
      }
      if (m_sent < m_total)
      {
        Time tNext(Seconds(0.01));
        m_sendEvent = Simulator::Schedule(tNext, &SenderApp::SendPacket, this);
      }
    }

  private:
    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_total;
    uint32_t m_sent;
    uint32_t m_pktSize;
    EventId m_sendEvent;
  };

  Ptr<SenderApp> app = CreateObject<SenderApp>();
  app->Setup(srcSocket, InetSocketAddress(if12.GetAddress(1), sinkPort), totalBytes, segmentSize);
  nodes.Get(0)->AddApplication(app);
  app->SetStartTime(Seconds(1.1));
  app->SetStopTime(Seconds(30.0));

  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("tcp-large-transfer.tr");
  p2p.EnableAsciiAll(stream);

  p2p.EnablePcapAll("tcp-large-transfer", false);

  Simulator::Stop(Seconds(30.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}