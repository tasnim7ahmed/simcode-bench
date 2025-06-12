#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpSimulation");

class ServerLoggingApp : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("ServerLoggingApp")
      .SetParent<Application>()
      .SetGroupName("Tutorial")
      .AddConstructor<ServerLoggingApp>();
    return tid;
  }

private:
  virtual void StartApplication(void)
  {
    TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
    Ptr<Socket> socket = Socket::CreateSocket(GetNode(), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 8080);
    socket->Bind(local);
    socket->Listen();
    socket->SetAcceptCallback(MakeCallback(&ServerLoggingApp::HandleAccept, this), MakeNullCallback<Ptr<Socket>, const Address&>());
  }

  void HandleAccept(Ptr<Socket> socket, const Address& from)
  {
    NS_LOG_INFO("Connection accepted from " << from);
    socket->SetRecvCallback(MakeCallback(&ServerLoggingApp::HandleRead, this));
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    while ((packet = socket->Recv()))
    {
      NS_LOG_INFO("Received packet of size " << packet->GetSize());
    }
  }
};

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("TcpSimulation", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Install server application
  ServerLoggingApp serverApp;
  serverApp.SetStartTime(Seconds(1.0));
  serverApp.SetStopTime(Seconds(10.0));
  nodes.Get(1)->AddApplication(&serverApp);

  // Create TCP client socket and send data
  Ptr<Socket> clientSocket;
  TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
  clientSocket = Socket::CreateSocket(nodes.Get(0), tid);
  InetSocketAddress remoteAddr(interfaces.GetAddress(1), 8080);
  clientSocket->Connect(remoteAddr);

  Simulator::Schedule(Seconds(2.0), &Socket::Send, clientSocket, Create<Packet>(1024), 0);

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}