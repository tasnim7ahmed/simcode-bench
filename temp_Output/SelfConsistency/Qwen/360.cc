#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteUdpSimulation");

class UdpServer : public Application
{
public:
  UdpServer();
  virtual ~UdpServer();

  static TypeId GetTypeId(void);
  void Setup(Ptr<Socket> socket, uint16_t port);

protected:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

private:
  void HandleRead(Ptr<Socket> socket);

  Ptr<Socket> m_socket;
  uint16_t m_port;
};

UdpServer::UdpServer()
    : m_socket(0), m_port(0)
{
}

UdpServer::~UdpServer()
{
  m_socket = nullptr;
}

TypeId UdpServer::GetTypeId(void)
{
  static TypeId tid = TypeId("UdpServer")
                          .SetParent<Application>()
                          .AddConstructor<UdpServer>();
  return tid;
}

void UdpServer::Setup(Ptr<Socket> socket, uint16_t port)
{
  m_socket = socket;
  m_port = port;
  m_socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), m_port));
  m_socket->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));
}

void UdpServer::StartApplication(void)
{
  NS_LOG_INFO("Starting server at " << Simulator::Now().GetSeconds() << "s");
}

void UdpServer::StopApplication(void)
{
  NS_LOG_INFO("Stopping server at " << Simulator::Now().GetSeconds() << "s");
}

void UdpServer::HandleRead(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
  {
    NS_LOG_INFO("Server received " << packet->GetSize() << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4()
                                   << " at time " << Simulator::Now().GetSeconds() << "s");

    // Echo the packet back to sender
    socket->SendTo(packet, 0, from);
  }
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  // Enable logging
  LogComponentEnable("LteUdpSimulation", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  Config::SetDefault("ns3::LteHelper::UseIdealRrc", BooleanValue(true));

  // Create LTE helper
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
  lteHelper->SetEpcHelper(epcHelper);

  // Create nodes: one eNB and one UE
  NodeContainer enbNodes;
  enbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(1);

  // Install LTE devices
  NetDeviceContainer enbDevs;
  enbDevs = lteHelper->InstallEnbDevice(enbNodes);

  NetDeviceContainer ueDevs;
  ueDevs = lteHelper->InstallUeDevice(ueNodes);

  // Attach UE to eNB
  lteHelper->Attach(ueDevs, enbDevs.Get(0));

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install(ueNodes);
  internet.Install(enbNodes);

  // Assign IP addresses
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

  Ipv4Address enbAddr = epcHelper->GetS1apSgw()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
  Ipv4InterfaceContainer enbIpIface;
  enbIpIface.Add(epcHelper->GetS1apSgw(), Ipv4InterfaceAddress(enbAddr, Ipv4Mask("/24")));

  NS_LOG_INFO("UE IP address: " << ueIpIface.GetAddress(0, 0));
  NS_LOG_INFO("eNB IP address: " << enbAddr);

  // Set up UDP server on eNB
  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  Ptr<Socket> recvSink = Socket::CreateSocket(enbNodes.Get(0), tid);
  uint16_t port = 8000;
  UdpServerHelper serverHelper;
  serverHelper.Setup(recvSink, port);
  enbNodes.Get(0)->AddApplication(serverHelper.Install(enbNodes.Get(0)));
  enbNodes.Get(0)->GetApplication(0)->SetStartTime(Seconds(1.0));
  enbNodes.Get(0)->GetApplication(0)->SetStopTime(Seconds(10.0));

  // Set up UDP client on UE
  Ptr<Socket> source = Socket::CreateSocket(ueNodes.Get(0), tid);
  InetSocketAddress destAddr(enbAddr, port);
  destAddr.SetTos(0xb8); // DSCP AF41 (low drop precedence for video streaming)

  source->Connect(destAddr);

  auto app = DynamicCast<UdpClient>(source->GetNode()->GetApplication(0));
  if (!app)
  {
    app = CreateObject<UdpClient>();
    app->SetAttribute("Interval", TimeValue(Seconds(0.1)));
    app->SetAttribute("Remote", AddressValue(destAddr));
    app->SetAttribute("PacketSize", UintegerValue(1024));
    ueNodes.Get(0)->AddApplication(app);
  }

  app->SetStartTime(Seconds(2.0));
  app->SetStopTime(Seconds(10.0));

  // Enable tracing
  lteHelper->EnableTraces();

  // Run simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}