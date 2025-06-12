#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"
#include "ns3/ocb-wifi-mac.h"
#include "ns3/wifi-80211p-helper.h"
#include "ns3/propagation-module.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-phy.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetSimulation");

int main(int argc, char *argv[]) {
  bool verbose = false;
  bool tracing = false;
  uint32_t nVehicles = 5;
  double simulationTime = 10;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if true", verbose);
  cmd.AddValue("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue("nVehicles", "Number of vehicles", nVehicles);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("VanetSimulation", LOG_LEVEL_INFO);
  }

  NodeContainer vehicles;
  vehicles.Create(nVehicles);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                "X", StringValue("100.0"),
                                "Y", StringValue("100.0"),
                                "Rho", StringValue("ns3::UniformRandomVariable[Min=0|Max=30]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(0, 200, 0, 200)),
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=10]"));
  mobility.Install(vehicles);

  YansWifiChannelHelper wifiChannelHelper = YansWifiChannelHelper::Default();
  YansWifiPhyHelper wifiPhyHelper = YansWifiPhyHelper::Default();
  wifiPhyHelper.SetChannel(wifiChannelHelper.Create());

  NqosWaveMacHelper waveMacHelper = NqosWaveMacHelper::Default();
  Wifi80211pHelper wifi80211pHelper = Wifi80211pHelper::Default();
  NetDeviceContainer waveDevices = wifi80211pHelper.Install(wifiPhyHelper, waveMacHelper, vehicles);

  InternetStackHelper internet;
  internet.Install(vehicles);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.10.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign(waveDevices);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(vehicles.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simulationTime));

  UdpEchoClientHelper echoClient(i.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(100000));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps;
  for (uint32_t j = 1; j < vehicles.GetN(); ++j) {
      clientApps.Add(echoClient.Install(vehicles.Get(j)));
  }
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(simulationTime));

  if (tracing) {
    wifiPhyHelper.EnablePcap("vanet", waveDevices);
  }

  AnimationInterface anim("vanet.xml");

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}