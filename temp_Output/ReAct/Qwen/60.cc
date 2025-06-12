#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpLargeTransferSimulation");

class TcpSocketTrace
{
public:
  static void CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal)
  {
    *stream->GetStream() << Simulator::Now().GetSeconds() << " " << newVal << std::endl;
  }

  static void RxDrop(Ptr<PcapFileWrapper> file, Ptr<const Packet> p)
  {
    NS_LOG_UNCOND("Packet drop");
    file->Write(Simulator::Now(), p);
  }
};

int main(int argc, char *argv[])
{
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpSocketFactory::GetTypeId()));

  NodeContainer nodes;
  nodes.Create(3);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
  pointToPoint.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));

  NetDeviceContainer devices01;
  devices01 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));

  NetDeviceContainer devices12;
  devices12 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces01 = address.Assign(devices01);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t sinkPort = 8080;
  Address sinkAddress(InetSocketAddress(interfaces12.GetAddress(1), sinkPort));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(2));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(10.0));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());

  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> cwndStream = asciiTraceHelper.CreateFileStream("tcp-large-transfer.tr");
  ns3TcpSocket->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback(&TcpSocketTrace::CwndChange, cwndStream));

  TypeId tid = TypeId::LookupByName("ns3::TcpSocketBase");
  ns3TcpSocket->SetAttribute("TypeId", TypeIdValue(tid));

  class MyApp : public Application
  {
  public:
    MyApp();
    virtual ~MyApp();

    void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t totalBytes);
    virtual void StartApplication(void);
    virtual void StopApplication(void);

  private:
    void SendPacket(void);

    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_packetSize;
    uint32_t m_totalBytes;
    uint32_t m_sentBytes;
  };

  MyApp::MyApp()
      : m_socket(0),
        m_peer(),
        m_packetSize(0),
        m_totalBytes(0),
        m_sentBytes(0)
  {
  }

  MyApp::~MyApp()
  {
    m_socket = nullptr;
  }

  void MyApp::Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t totalBytes)
  {
    m_socket = socket;
    m_peer = address;
    m_packetSize = packetSize;
    m_totalBytes = totalBytes;
  }

  void MyApp::StartApplication()
  {
    m_sentBytes = 0;
    m_socket->Bind();
    m_socket->Connect(m_peer);
    SendPacket();
  }

  void MyApp::StopApplication()
  {
    if (m_socket)
    {
      m_socket->Close();
    }
  }

  void MyApp::SendPacket()
  {
    while (true)
    {
      uint32_t toSend = m_totalBytes - m_sentBytes;
      if (toSend == 0)
      {
        break;
      }
      uint32_t thisSend = std::min(toSend, m_packetSize);
      Ptr<Packet> packet = Create<Packet>(thisSend);
      int actual = m_socket->Send(packet);
      if (actual <= 0)
      {
        return;
      }
      m_sentBytes += actual;
    }
  }

  Ptr<MyApp> app = CreateObject<MyApp>();
  app->Setup(ns3TcpSocket, sinkAddress, 1024, 2000000);
  nodes.Get(0)->AddApplication(app);
  app->SetStartTime(Seconds(1.0));
  app->SetStopTime(Seconds(10.0));

  pointToPoint.EnablePcapAll("tcp-large-transfer");

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}