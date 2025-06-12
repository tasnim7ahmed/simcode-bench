#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteUdpSimulation");

class UePacketSink : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("UePacketSink")
      .SetParent<Application>()
      .AddConstructor<UePacketSink>();
    return tid;
  }

  UePacketSink() {}
  virtual ~UePacketSink() {}

private:
  virtual void StartApplication(void)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    Ptr<Socket> socket = Socket::CreateSocket(GetNode(), tid);

    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 5000);
    socket->Bind(local);
    socket->SetRecvCallback(MakeCallback(&UePacketSink::ReceivePacket, this));
  }

  virtual void StopApplication(void) {}

  void ReceivePacket(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      NS_LOG_INFO("Received packet of size " << packet->GetSize());
    }
  }
};

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::LteHelper::UseRlcSm", BooleanValue(true));
  Config::SetDefault("ns3::LteHelper::AnrEnabled", BooleanValue(false));
  Config::SetDefault("ns3::LteHelper::UseIdealRrc", BooleanValue(true));

  Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
  lteHelper->SetEpcHelper(epcHelper);
  lteHelper->SetAttribute("PathlossModel", StringValue("ns3::FriisSpectrumPropagationLossModel"));

  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(1);

  InternetStackHelper internet;
  internet.Install(ueNodes);
  internet.Install(enbNodes);

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer ueIpIfaces = ipv4h.Assign(epcHelper->AssignUeIpv4Address(NetDeviceContainer()));
  Ipv4Address ueAddr = ueIpIfaces.GetAddress(0);

  lteHelper->Attach(ueNodes.Get(0), enbNodes.Get(0));

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(enbNodes);

  mobility.SetMobilityModel("ns3::RandomWalk2DMobilityModel",
                            "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
  mobility.Install(ueNodes);

  ApplicationContainer sinkApp;
  sinkApp.Add(CreateObject<UePacketSink>());
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  UdpClientHelper client(ueAddr, 5000);
  client.SetAttribute("MaxPackets", UintegerValue(50)); // 10s / 0.2s = 50 packets
  client.SetAttribute("Interval", TimeValue(MilliSeconds(200)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = client.Install(enbNodes);
  clientApps.Start(Seconds(0.0));
  clientApps.Stop(Seconds(10.0));

  lteHelper->EnableTraces();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}