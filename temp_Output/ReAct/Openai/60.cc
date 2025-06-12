#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

void
CwndTracer(uint32_t oldCwnd, uint32_t newCwnd)
{
  static std::ofstream cwndStream("cwnd-trace.txt", std::ios_base::app);
  cwndStream << Simulator::Now().GetSeconds() << " " << newCwnd << std::endl;
}

int
main(int argc, char *argv[])
{
  // Create nodes
  NodeContainer nodes;
  nodes.Create(3);

  // Set up point-to-point links between n0<->n1 and n1<->n2
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("10ms"));

  NetDeviceContainer d0d1 = p2p.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer d1d2 = p2p.Install(nodes.Get(1), nodes.Get(2));

  // Enable pcap tracing on each device
  p2p.EnablePcap("tcp-large-transfer-0-0", d0d1.Get(0), true, true);
  p2p.EnablePcap("tcp-large-transfer-1-0", d0d1.Get(1), true, true);
  p2p.EnablePcap("tcp-large-transfer-1-1", d1d2.Get(0), true, true);
  p2p.EnablePcap("tcp-large-transfer-2-0", d1d2.Get(1), true, true);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper addr;
  addr.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = addr.Assign(d0d1);

  addr.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = addr.Assign(d1d2);

  // Enable queue tracing
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("tcp-large-transfer.tr");
  p2p.EnableAsciiAll(stream);

  // Install TCP receiver (PacketSink) on node n2
  uint16_t sinkPort = 8080;
  Address sinkAddress(InetSocketAddress(i1i2.GetAddress(1), sinkPort));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(2));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(30.0));

  // Create and configure the TCP sender at n0
  Ptr<Node> srcNode = nodes.Get(0);
  Ptr<Socket> srcSocket = Socket::CreateSocket(srcNode, TcpSocketFactory::GetTypeId());

  // Configure Congestion Window trace
  srcSocket->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&CwndTracer));

  Address serverAddr = InetSocketAddress(i1i2.GetAddress(1), sinkPort);

  // Define sending application using raw socket
  class TcpSenderApp : public Application
  {
  public:
    TcpSenderApp() :
      m_socket(0),
      m_peer(),
      m_totalBytes(0),
      m_sentBytes(0),
      m_sendSize(1000),
      m_running(false)
    {}
    void Setup(Ptr<Socket> socket, Address peer, uint32_t totalBytes)
    {
      m_socket = socket;
      m_peer = peer;
      m_totalBytes = totalBytes;
    }

  private:
    virtual void StartApplication()
    {
      m_running = true;
      m_sentBytes = 0;
      m_socket->Bind();
      m_socket->Connect(m_peer);
      m_socket->SetSendCallback(MakeCallback(&TcpSenderApp::DataSend, this));
      SendPacket();
    }

    virtual void StopApplication()
    {
      m_running = false;
      if (m_socket)
      {
        m_socket->Close();
      }
    }

    void SendPacket()
    {
      while (m_running && m_socket->GetTxAvailable() > 0 && m_sentBytes < m_totalBytes)
      {
        uint32_t left = m_totalBytes - m_sentBytes;
        uint32_t toSend = std::min(left, m_sendSize);
        Ptr<Packet> packet = Create<Packet>(toSend);
        int actual = m_socket->Send(packet);
        if (actual > 0)
        {
          m_sentBytes += actual;
        }
        else
        {
          break;
        }
      }
    }

    void DataSend(Ptr<Socket>, uint32_t)
    {
      if (m_sentBytes < m_totalBytes)
      {
        Simulator::ScheduleNow(&TcpSenderApp::SendPacket, this);
      }
    }

    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_totalBytes;
    uint32_t m_sentBytes;
    uint32_t m_sendSize;
    bool m_running;
  };

  Ptr<TcpSenderApp> senderApp = CreateObject<TcpSenderApp>();
  senderApp->Setup(srcSocket, serverAddr, 2000000);

  nodes.Get(0)->AddApplication(senderApp);
  senderApp->SetStartTime(Seconds(1.0));
  senderApp->SetStopTime(Seconds(30.0));

  // Trace packet reception events on the PacketSink
  sinkApp.Get(0)->TraceConnectWithoutContext("Rx", MakeBoundCallback(
    [](Ptr<const Packet> packet, const Address &addr) {
      static std::ofstream rxTrace("tcp-large-transfer.rx", std::ios_base::app);
      rxTrace << Simulator::Now().GetSeconds() << " " << packet->GetSize() << std::endl;
    }
  ));

  Simulator::Stop(Seconds(30.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}