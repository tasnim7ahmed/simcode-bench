#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-echo-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiSimple");

int main(int argc, char *argv[]) {
  bool verbose = false;
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application containers to log if set to true", verbose);
  cmd.AddValue("tracing", "Enable pcap tracing", tracing);

  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
  }

  NodeContainer staNodes;
  staNodes.Create(1);

  NodeContainer apNodes;
  apNodes.Create(1);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid("ns3-wifi");
  wifiMac.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(ssid),
                  "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, staNodes);

  wifiMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid),
                  "BeaconInterval", TimeValue(Seconds(1.0)));
  NetDeviceContainer apDevices = wifi.Install(wifiPhy, wifiMac, apNodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNodes);
  mobility.Install(staNodes);

  InternetStackHelper stack;
  stack.Install(apNodes);
  stack.Install(staNodes);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(apNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(apInterfaces.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(staNodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  if (tracing) {
    wifiPhy.EnablePcap("wifi-simple", apDevices);
    wifiPhy.EnablePcap("wifi-simple", staDevices);
  }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}