#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessHandoverSimulation");

class HandoverStats {
public:
  uint32_t handovers = 0;
  uint32_t packetLoss = 0;
};

void
NotifyHandover(std::string context, Mac48Address oldAp, Mac48Address newAp, HandoverStats* stats)
{
  if (oldAp != newAp) {
    NS_LOG_INFO(context << " Handing over from " << oldAp << " to " << newAp);
    stats->handovers++;
  }
}

void
PacketDropTrace(Ptr<const Packet> p, const WifiMacHeader *header, WifiPhyRxfailureReason reason, HandoverStats* stats)
{
  stats->packetLoss++;
}

int main(int argc, char *argv[])
{
  uint32_t numNodes = 6;
  double simDuration = 10.0; // seconds

  CommandLine cmd(__FILE__);
  cmd.AddValue("numNodes", "Number of mobile nodes", numNodes);
  cmd.AddValue("simDuration", "Simulation duration in seconds", simDuration);
  cmd.Parse(argc, argv);

  NodeContainer apNodes;
  apNodes.Create(2);

  NodeContainer staNodes;
  staNodes.Create(numNodes);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetRemoteStationManager("ns3::AarfcdWifiManager");

  WifiMacHelper mac;
  Ssid ssid1 = Ssid("networkOne");
  Ssid ssid2 = Ssid("networkTwo");

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid1),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices1;
  staDevices1 = wifi.Install(phy, mac, staNodes);

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid2),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices2;
  staDevices2 = wifi.Install(phy, mac, staNodes);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid1));
  NetDeviceContainer apDevices1;
  apDevices1 = wifi.Install(phy, mac, apNodes.Get(0));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid2));
  NetDeviceContainer apDevices2;
  apDevices2 = wifi.Install(phy, mac, apNodes.Get(1));

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(5.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
  mobility.Install(staNodes);

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNodes);

  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces1 = address.Assign(apDevices1);
  Ipv4InterfaceContainer apInterfaces2 = address.Assign(apDevices2);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces1 = address.Assign(staDevices1);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces2 = address.Assign(staDevices2);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(staNodes.Get(0));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(simDuration));

  UdpEchoClientHelper echoClient(staInterfaces1.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  echoClient.SetAttribute("PacketSize", UintegerValue(512));

  ApplicationContainer clientApps;
  for (uint32_t i = 1; i < numNodes; ++i)
    {
      clientApps.Add(echoClient.Install(staNodes.Get(i)));
    }
  clientAds.Start(Seconds(2.0));
  clientAds.Stop(Seconds(simDuration - 1));

  HandoverStats stats;
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/EndTx", MakeBoundCallback(&PacketDropTrace, &stats));
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/LinkUp", MakeBoundCallback(&NotifyHandover, &stats));

  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("wireless-handover.tr");
  phy.EnableAsciiAll(stream);

  AnimationInterface anim("wireless-handover.xml");
  anim.SetMobilityPollInterval(Seconds(0.5));

  Simulator::Stop(Seconds(simDuration));
  Simulator::Run();

  std::cout << "Total Handovers: " << stats.handovers << std::endl;
  std::cout << "Total Packet Loss: " << stats.packetLoss << std::endl;

  Simulator::Destroy();
  return 0;
}