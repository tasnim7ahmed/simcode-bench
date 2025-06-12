#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"
#include "ns3/command-line.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/wave-module.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/vector.h"
#include <fstream>
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VehicularSimulation");

int main(int argc, char *argv[]) {
  CommandLine cmd;
  bool enablePcap = false;
  double simulationTime = 10.0;
  std::string scenario = "urban";

  cmd.AddValue("pcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue("time", "Simulation time (s)", simulationTime);
  cmd.AddValue("scenario", "urban or highway", scenario);
  cmd.Parse(argc, argv);

  LogComponentEnable("VehicularSimulation", LOG_LEVEL_INFO);

  NodeContainer vehicles;
  vehicles.Create(2);

  MobilityHelper mobility;
  if (scenario == "urban") {
      mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                  "X", StringValue("50.0"),
                                  "Y", StringValue("50.0"),
                                  "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));

      mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Mode", StringValue("Time"),
                                "Time", StringValue("1s"),
                                "Speed", StringValue("ns3::ConstantRandomVariable[Constant=10.0]"),
                                "Bounds", StringValue("0|100|0|100"));
  } else if (scenario == "highway"){
      mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue(0.0),
                                   "MinY", DoubleValue(0.0),
                                   "DeltaX", DoubleValue(5.0),
                                   "DeltaY", DoubleValue(0.0),
                                   "GridWidth", UintegerValue(2),
                                   "LayoutType", StringValue("RowFirst"));

      mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  } else {
      NS_LOG_ERROR("Invalid scenario. Choose 'urban' or 'highway'.");
      return 1;
  }
  mobility.Install(vehicles);

  if (scenario == "highway") {
    Ptr<ConstantVelocityMobilityModel> mob0 = vehicles.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    mob0->SetVelocity(Vector(20, 0, 0));

    Ptr<ConstantVelocityMobilityModel> mob1 = vehicles.Get(1)->GetObject<ConstantVelocityMobilityModel>();
    mob1->SetVelocity(Vector(25, 0, 0));
  }

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211_AD);

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, vehicles.Get(0));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, vehicles.Get(1));

  InternetStackHelper internet;
  internet.Install(vehicles);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces;
  interfaces = address.Assign(NetDeviceContainer::Create(staDevices, apDevices));

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(vehicles.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simulationTime));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(vehicles.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(simulationTime));

  std::ofstream snrFile("snr.txt");
  std::ofstream pathlossFile("pathloss.txt");

  Simulator::Schedule(Seconds(0.1), [&](){
    for (double t = 0.1; t < simulationTime; t += 0.1) {
      Simulator::Schedule(Seconds(t), [&,t](){
        Ptr<MobilityModel> mob0 = vehicles.Get(0)->GetObject<MobilityModel>();
        Ptr<MobilityModel> mob1 = vehicles.Get(1)->GetObject<MobilityModel>();

        Vector pos0 = mob0->GetPosition();
        Vector pos1 = mob1->GetPosition();

        double distance = std::sqrt(std::pow(pos0.x - pos1.x, 2) + std::pow(pos0.y - pos1.y, 2) + std::pow(pos0.z - pos1.z, 2));

         // TR 38.901 pathloss model (simplified)
        double frequency = 28e9; // 28 GHz
        double pathLoss = 32.4 + 20 * std::log10(frequency/1e9) + 20 * std::log10(distance);

        // Simplified SNR calculation (assuming constant transmit power and noise floor)
        double transmitPower = 20.0; // dBm
        double noiseFloor = -90.0; // dBm
        double snr = transmitPower - pathLoss - noiseFloor;

        snrFile << t << " " << snr << std::endl;
        pathlossFile << t << " " << pathLoss << std::endl;
      });
    }
  });

  if (enablePcap) {
    phy.EnablePcapAll("vehicular");
  }

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  snrFile.close();
  pathlossFile.close();

  Simulator::Destroy();
  return 0;
}