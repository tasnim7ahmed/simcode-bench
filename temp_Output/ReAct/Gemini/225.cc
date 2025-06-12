#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/ocb-wifi-mac.h"
#include "ns3/wifi-80211p-helper.h"
#include "ns3/wave-net-device.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer vehicles;
  vehicles.Create(10);

  NodeContainer rsu;
  rsu.Create(1);

  NodeContainer allNodes;
  allNodes.Add(vehicles);
  allNodes.Add(rsu);

  WifiHelper wifiHelper;
  wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211p);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.Set("TxPowerStart", DoubleValue(33));
  wifiPhy.Set("TxPowerEnd", DoubleValue(33));
  wifiPhy.Set("TxPowerLevels", UintegerValue(1));
  wifiPhy.Set("EnergyDetectionThreshold", DoubleValue(-99));
  wifiPhy.Set("CcaEdThreshold", DoubleValue(-96));
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel(wifiChannel.Create());

  NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default();
  Wifi80211pHelper wifi80211pHelper;
  NetDeviceContainer devices = wifi80211pHelper.Install(wifiPhy, waveMac, allNodes);

  DsdvHelper dsdv;
  InternetStackHelper stack;
  stack.SetRoutingHelper(dsdv);
  stack.Install(allNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(10),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::WaypointMobilityModel");
  mobility.Install(vehicles);

  for (uint32_t i = 0; i < vehicles.GetN(); ++i) {
    Ptr<Node> vehicleNode = vehicles.Get(i);
    Ptr<WaypointMobilityModel> mobilityModel = vehicleNode->GetObject<WaypointMobilityModel>();
    Waypoint wp;
    wp.time = Seconds(0.0);
    wp.position = Vector(i * 10, 0, 0);
    mobilityModel->AddWaypoint(wp);
    wp.time = Seconds(30.0);
    wp.position = Vector(i * 10 + 50, 0, 0);
    mobilityModel->AddWaypoint(wp);
  }

  Ptr<Node> rsuNode = rsu.Get(0);
  Ptr<MobilityModel> rsuMobility = rsuNode->GetObject<MobilityModel>();
  rsuMobility->SetPosition(Vector(50, 50, 0));

  uint16_t port = 9;

  UdpServerHelper server(port);
  server.SetAttribute("StartTime", TimeValue(Seconds(0.0)));
  ApplicationContainer serverApps = server.Install(rsu.Get(0));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(30.0));

  UdpClientHelper client(interfaces.GetAddress(vehicles.GetN()), port);
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client.SetAttribute("MaxPackets", UintegerValue(30));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < vehicles.GetN(); ++i) {
      ApplicationContainer temp = client.Install(vehicles.Get(i));
      clientApps.Add(temp);
  }
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(30.0));

  AnimationInterface anim("vanet_dsdv.xml");

  Simulator::Stop(Seconds(30.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}