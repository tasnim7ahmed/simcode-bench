#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/nr-module.h"
#include "ns3/epc-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Nr5GEpcSimulation");

class Nr5GEpcSimulation : public Object
{
public:
  static TypeId GetTypeId(void);
};

TypeId
Nr5GEpcSimulation::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::Nr5GEpcSimulation")
                          .SetParent<Object>()
                          .AddConstructor<Nr5GEpcSimulation>();
  return tid;
}

int main(int argc, char *argv[])
{
  // Log components
  LogComponentEnable("Nr5GEpcSimulation", LOG_LEVEL_INFO);

  // Simulation parameters
  double simDuration = 10.0; // seconds
  uint32_t packetSize = 512;
  Time interPacketInterval = MilliSeconds(500);

  // Set up NR and EPC helpers
  Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
  Ptr<EpcHelper> epcHelper = EpcHelper::GetEpcHelper("ns3::PointToPointEpcHelper");
  nrHelper->SetEpcHelper(epcHelper);
  nrHelper->SetSchedulerType("ns3::nr::bwp::GnbBwpManagerAlps");

  // Create gNB and UE nodes
  NodeContainer gNbNode;
  gNbNode.Create(1);
  NodeContainer ueNode;
  ueNode.Create(1);

  // Install EPC on the core network node
  NetDeviceContainer gnbNetDev = nrHelper->InstallGnbDevice(gNbNode, epcHelper->GetS1uGtpuSocketFactory());
  NetDeviceContainer ueNetDev = nrHelper->InstallUeDevice(ueNode, epcHelper->GetS1uGtpuSocketFactory());

  // Install internet stack
  InternetStackHelper internet;
  internet.Install(ueNode);
  internet.Install(gNbNode);

  // Assign IP addresses
  Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address(ueNetDev);
  Ipv4Address pgwAddr = epcHelper->GetPgwNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();

  // Attach UEs to gNBs
  for (uint32_t i = 0; i < ueNode.GetN(); ++i)
    {
      nrHelper->Attach(ueNetDev.Get(i), gnbNetDev.Get(0));
    }

  // Setup mobility
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(ueNode);
  ueNode.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(10, 0, 0));

  // gNB is stationary
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(gNbNode);

  // Install UDP server on gNB
  uint16_t dlPort = 1234;
  PacketSinkHelper dlPacketSinkHelper("ns3::UdpSocketFactory",
                                     InetSocketAddress(Ipv4Address::GetAny(), dlPort));
  ApplicationContainer dlSinkApp = dlPacketSinkHelper.Install(gNbNode);
  dlSinkApp.Start(Seconds(0.0));
  dlSinkApp.Stop(Seconds(simDuration));

  // Install UDP client on UE
  InetSocketAddress dst(pgwAddr, dlPort);
  dst.SetTos(0xb8); // DSCP AF41 (QCI 5)
  OnOffHelper onOffUdpClient("ns3::UdpSocketFactory", dst);
  onOffUdpClient.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=500]"));
  onOffUdpClient.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onOffUdpClient.SetAttribute("DataRate", DataRateValue(DataRate("100kbps")));
  onOffUdpClient.SetAttribute("PacketSize", UintegerValue(packetSize));

  ApplicationContainer dlClientApp = onOffUdpClient.Install(ueNode);
  dlClientApp.Start(Seconds(0.5));
  dlClientApp.Stop(Seconds(simDuration - 0.1));

  // Enable RLC layer statistics
  nrHelper->EnableRlcTraces();

  // Start simulation
  Simulator::Stop(Seconds(simDuration));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}