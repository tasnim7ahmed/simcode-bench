#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteUdpSimulation");

class UdpServerApp : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("UdpServerApp")
      .SetParent<Application>()
      .AddConstructor<UdpServerApp>()
      .AddAttribute("Port", "Listening port",
                    UintegerValue(12345),
                    MakeUintegerAccessor(&UdpServerApp::m_port),
                    MakeUintegerChecker<uint16_t>());
    return tid;
  }

  UdpServerApp() {}
  virtual ~UdpServerApp() {}

private:
  virtual void StartApplication(void)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    Ptr<Socket> socket = Socket::CreateSocket(GetNode(), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
    socket->Bind(local);
    socket->SetRecvCallback(MakeCallback(&UdpServerApp::HandleRead, this));
  }

  virtual void StopApplication(void) {}

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      // Process incoming packet
    }
  }

  uint16_t m_port;
};

int main(int argc, char *argv[])
{
  Config::SetDefault("ns3::LteHelper::UseIdealRrc", BooleanValue(true));
  Config::SetDefault("ns3::LteHelper::UseIdealRlc", BooleanValue(true));

  Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
  lteHelper->SetEpcHelper(epcHelper);

  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(1);

  lteHelper->SetSchedulerType("ns3::PfFfMacScheduler");

  NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
  NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

  InternetStackHelper internet;
  internet.Install(ueNodes);
  internet.Install(enbNodes);

  Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

  lteHelper->Attach(ueDevs, enbDevs.Get(0));

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(enbNodes);
  mobility.Install(ueNodes);

  // Server application on UE
  UdpServerApp serverApp;
  serverApp.SetTypeId("UdpServerApp");
  ueNodes.Get(0)->AddApplication(&serverApp);
  serverApp.SetStartTime(Seconds(0.1));
  serverApp.SetStopTime(Seconds(25.0));

  // Client application on eNB
  UdpClientHelper clientHelper(ueIpIfaces.GetAddress(0), 12345);
  clientHelper.SetAttribute("MaxPackets", UintegerValue(1000));
  clientHelper.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
  clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = clientHelper.Install(enbNodes.Get(0));
  clientApps.Start(Seconds(0.1));
  clientApps.Stop(Seconds(25.0));

  Simulator::Stop(Seconds(25.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}