#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class ReceiverSink : public Application
{
public:
  ReceiverSink() : m_receivedBytes(0) {}

  void Setup(Address address, uint16_t port)
  {
    m_localAddress = address;
    m_port = port;
  }

  uint64_t GetTotalRx() const
  {
    return m_receivedBytes;
  }

private:
  virtual void StartApplication() override
  {
    if (InetSocketAddress::IsMatchingType(m_localAddress))
    {
      m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
      InetSocketAddress inetAddr = InetSocketAddress::ConvertFrom(m_localAddress);
      m_socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), inetAddr.GetPort()));
      m_socket->Listen();
      m_socket->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address &>(),
                                  MakeCallback(&ReceiverSink::HandleAccept, this));
    }
  }

  virtual void StopApplication() override
  {
    if (m_socket)
    {
      m_socket->Close();
      m_socket = nullptr;
    }
    for (auto &sock : m_acceptedSockets)
    {
      sock->Close();
    }
    m_acceptedSockets.clear();
  }

  void HandleAccept(Ptr<Socket> s, const Address &from)
  {
    s->SetRecvCallback(MakeCallback(&ReceiverSink::HandleRead, this));
    m_acceptedSockets.push_back(s);
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    while ((packet = socket->Recv()))
    {
      m_receivedBytes += packet->GetSize();
    }
  }

  Address m_localAddress;
  uint16_t m_port;
  Ptr<Socket> m_socket;
  std::vector<Ptr<Socket>> m_acceptedSockets;
  uint64_t m_receivedBytes;
};

void PrintTotalRx(Ptr<ReceiverSink> sink)
{
  std::cout << "Total Bytes Received: " << sink->GetTotalRx() << std::endl;
}

int main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices = p2p.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 5000;

  Ptr<ReceiverSink> sinkApp = CreateObject<ReceiverSink>();
  Address localAddress(InetSocketAddress(interfaces.GetAddress(1), port));
  sinkApp->Setup(localAddress, port);
  nodes.Get(1)->AddApplication(sinkApp);
  sinkApp->SetStartTime(Seconds(0.0));
  sinkApp->SetStopTime(Seconds(10.0));

  BulkSendHelper bulk("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
  bulk.SetAttribute("MaxBytes", UintegerValue(0));
  ApplicationContainer sourceApps = bulk.Install(nodes.Get(0));
  sourceApps.Start(Seconds(0.1));
  sourceApps.Stop(Seconds(10.0));

  p2p.EnablePcapAll("p2p-tcp-bulk");

  Simulator::Schedule(Seconds(10.01), &PrintTotalRx, sinkApp);

  Simulator::Stop(Seconds(10.02));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}