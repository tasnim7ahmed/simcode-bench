#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/command-line.h"
#include "ns3/uinteger.h"
#include "ns3/on-off-application.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  bool enableRtsCts = false;
  uint32_t payloadSize = 1472;
  double simulationTime = 10.0;
  double stationApDistance = 10.0;

  CommandLine cmd;
  cmd.AddValue("enableRtsCts", "Enable or disable RTS/CTS (default: false)", enableRtsCts);
  cmd.AddValue("payloadSize", "Payload size in bytes (default: 1472)", payloadSize);
  cmd.AddValue("simulationTime", "Simulation time in seconds (default: 10.0)", simulationTime);
  cmd.AddValue("stationApDistance", "Distance between station and AP (default: 10.0)", stationApDistance);
  cmd.Parse(argc, argv);

  NodeContainer apNodes;
  apNodes.Create(4);

  NodeContainer staNodes;
  staNodes.Create(4);

  YansWifiChannelHelper channel[4];
  YansWifiPhyHelper phy[4];
  WifiHelper wifi[4];
  NqosWifiMacHelper mac[4];

  std::string ssidStr[4] = {"network-A", "network-B", "network-C", "network-D"};
  Ssid ssid[4];

  for (int i = 0; i < 4; ++i) {
    ssid[i] = Ssid(ssidStr[i]);
    channel[i] = YansWifiChannelHelper::Default();
    phy[i] = YansWifiPhyHelper::Default();
    phy[i].SetChannel(channel[i].Create());

    wifi[i] = WifiHelper::Default();
    wifi[i].SetStandard(WIFI_PHY_STANDARD_80211n);

    mac[i] = NqosWifiMacHelper::Default();
  }

  NetDeviceContainer apDevice[4];
  NetDeviceContainer staDevice[4];

  for (int i = 0; i < 4; ++i) {
    mac[i].SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid[i]),
                    "BeaconGeneration", BooleanValue(true),
                    "BeaconInterval", TimeValue(Seconds(0.1)));

    apDevice[i] = wifi[i].Install(phy[i], mac[i], apNodes.Get(i));

    mac[i].SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid[i]),
                    "ActiveProbing", BooleanValue(false));

    staDevice[i] = wifi[i].Install(phy[i], mac[i], staNodes.Get(i));
  }

  if (enableRtsCts) {
      Config::Set("/NodeList/*/$ns3::WifiNetDevice/Mac/BE_Txop/RtsCtsThreshold", StringValue("0"));
  }

  Config::Set("/NodeList/*/$ns3::WifiNetDevice/Mac/BE_Txop/AlwaysFragment", BooleanValue(false));

  // Station A: Default aggregation (A-MSDU disabled, A-MPDU enabled at 65 KB)
  Config::Set("/NodeList/1/$ns3::WifiNetDevice/Mac/BE_Txop/AggregatedFrameMaxAmsduSize", UintegerValue(0));
  Config::Set("/NodeList/1/$ns3::WifiNetDevice/Mac/BE_Txop/AggregatedFrameMaxAmpduSize", UintegerValue(65535)); //65KB

  // Station B: A-MPDU and A-MSDU disabled
  Config::Set("/NodeList/3/$ns3::WifiNetDevice/Mac/BE_Txop/AggregatedFrameMaxAmsduSize", UintegerValue(0));
  Config::Set("/NodeList/3/$ns3::WifiNetDevice/Mac/BE_Txop/AggregatedFrameMaxAmpduSize", UintegerValue(0));

  // Station C: A-MSDU enabled with 8 KB, A-MPDU disabled
   Config::Set("/NodeList/5/$ns3::WifiNetDevice/Mac/BE_Txop/AggregatedFrameMaxAmsduSize", UintegerValue(8192)); //8KB
   Config::Set("/NodeList/5/$ns3::WifiNetDevice/Mac/BE_Txop/AggregatedFrameMaxAmpduSize", UintegerValue(0));

  // Station D: Two-level aggregation (A-MPDU at 32 KB, A-MSDU at 4 KB)
   Config::Set("/NodeList/7/$ns3::WifiNetDevice/Mac/BE_Txop/AggregatedFrameMaxAmsduSize", UintegerValue(4096)); //4KB
   Config::Set("/NodeList/7/$ns3::WifiNetDevice/Mac/BE_Txop/AggregatedFrameMaxAmpduSize", UintegerValue(32768)); //32KB

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(stationApDistance * 2),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNodes);

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(stationApDistance),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(stationApDistance * 2),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(staNodes);

  InternetStackHelper stack;
  stack.Install(apNodes);
  stack.Install(staNodes);

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer apInterface[4];
  Ipv4InterfaceContainer staInterface[4];

  address.SetBase("10.1.1.0", "255.255.255.0");
  apInterface[0] = address.Assign(apDevice[0]);
  staInterface[0] = address.Assign(staDevice[0]);

  address.SetBase("10.1.2.0", "255.255.255.0");
  apInterface[1] = address.Assign(apDevice[1]);
  staInterface[1] = address.Assign(staDevice[1]);

  address.SetBase("10.1.3.0", "255.255.255.0");
  apInterface[2] = address.Assign(apDevice[2]);
  staInterface[2] = address.Assign(staDevice[2]);

  address.SetBase("10.1.4.0", "255.255.255.0");
  apInterface[3] = address.Assign(apDevice[3]);
  staInterface[3] = address.Assign(staDevice[3]);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(apNodes);
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simulationTime));

  UdpEchoClientHelper echoClient(apInterface[0].GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(0xffffffff));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.001)));
  echoClient.SetAttribute("PacketSize", UintegerValue(payloadSize));

  ApplicationContainer clientApps[4];
  for (int i = 0; i < 4; ++i) {
     UdpEchoClientHelper client(apInterface[i].GetAddress(0), 9);
     client.SetAttribute("MaxPackets", UintegerValue(0xffffffff));
     client.SetAttribute("Interval", TimeValue(Seconds(0.001)));
     client.SetAttribute("PacketSize", UintegerValue(payloadSize));
     clientApps[i] = client.Install(staNodes.Get(i));
     clientApps[i].Start(Seconds(2.0));
     clientApps[i].Stop(Seconds(simulationTime));
  }

  Simulator::Stop(Seconds(simulationTime + 1));

  phy[0].Set("ChannelNumber", UintegerValue(36));
  phy[1].Set("ChannelNumber", UintegerValue(40));
  phy[2].Set("ChannelNumber", UintegerValue(44));
  phy[3].Set("ChannelNumber", UintegerValue(48));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}