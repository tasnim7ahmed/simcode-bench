#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpRttSimulation");

class RttTrace : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("RttTrace")
      .SetParent<Application>()
      .AddConstructor<RttTrace>();
    return tid;
  }

  RttTrace() {}
  virtual ~RttTrace() {}

protected:
  void DoInitialize() override
  {
    m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
    m_socket->Bind();
    m_socket->Connect(m_peer);
    m_socket->SetRecvCallback(MakeCallback(&RttTrace::RecvData, this));
    m_socket->SetSendCallback(MakeCallback(&RttTrace::DataSent, this));
  }

  void DoDispose() override
  {
    m_socket->Close();
    m_socket = nullptr;
    Application::DoDispose();
  }

private:
  void StartApplication() override
  {
    // Send initial data to start communication
    uint8_t *pkt = new uint8_t[1];
    pkt[0] = 'R';
    m_socket->Send(pkt, 1, 0);
    delete[] pkt;
    m_seq = 1;
    m_lastSentSeq = 1;
    m_rttStart = Simulator::Now();
  }

  void StopApplication() override {}

  void RecvData(Ptr<Socket> socket, Ptr<const Packet> packet, const Address &from)
  {
    TcpHeader tcpHdr;
    if (packet->PeekHeader(tcpHdr))
    {
      SequenceNumber32 ack = tcpHdr.GetAckNumber();
      if (ack == m_lastSentSeq)
      {
        Time rtt = Simulator::Now() - m_rttStart;
        NS_LOG_UNCOND("Measured RTT: " << rtt.GetMilliSeconds() << " ms");
      }
    }
  }

  void DataSent(Ptr<Socket> socket, uint32_t txSpace)
  {
    // After sending, wait for ACK before sending again to measure RTT
    m_lastSentSeq = m_seq;
    m_rttStart = Simulator::Now();
  }

  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_seq;
  uint32_t m_lastSentSeq;
  Time m_rttStart;
};

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t sinkPort = 8080;
  Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));

  // Install server application (PacketSink)
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
  sinkApps.Start(Seconds(1.0));
  sinkApps.Stop(Seconds(10.0));

  // Install client application with RTT tracing
  RttTrace rttClient;
  rttClient.SetRemote(sinkAddress);
  ApplicationContainer clientApp;
  clientApp.Add(rttClient.Install(nodes.Get(0)));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}