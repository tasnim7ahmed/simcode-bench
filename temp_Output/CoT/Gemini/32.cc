#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

int main(int argc, char *argv[]) {
  bool verbose = false;
  uint32_t packetSize = 1472;
  double simulationTime = 10;
  double startSNR = 5.0;
  double endSNR = 30.0;
  double snrStep = 1.0;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if set to true", verbose);
  cmd.AddValue("packetSize", "Size of packet sent in bytes", packetSize);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("startSNR", "Start SNR value", startSNR);
  cmd.AddValue("endSNR", "End SNR value", endSNR);
  cmd.AddValue("snrStep", "SNR step size", snrStep);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  }

  NodeContainer apNode;
  apNode.Create(1);

  NodeContainer staNode;
  staNode.Create(1);

  WifiHelper wifiHelper;
  wifiHelper.SetStandard(WIFI_STANDARD_80211_HE);

  YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();
  phyHelper.SetChannel(channelHelper.Create());

  wifiHelper.SetRemoteStationManager("ns3::HeWifiRemoteStationManager",
                                     "NonErpDataRate", StringValue("HE_MCS0"),
                                     "HeGuardInterval", StringValue("LTF1_GI0_8"));

  WifiMacHelper macHelper;
  Ssid ssid = Ssid("ns-3-ssid");
  macHelper.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevice = wifiHelper.Install(phyHelper, macHelper, apNode);

  macHelper.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevice = wifiHelper.Install(phyHelper, macHelper, staNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNode);
  mobility.Install(staNode);

  InternetStackHelper stack;
  stack.Install(apNode);
  stack.Install(staNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
  Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(apNode);
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simulationTime + 1));

  UdpClientHelper client(apInterface.GetAddress(0), 9);
  client.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client.SetAttribute("Interval", TimeValue(Time("0.00002")));
  client.SetAttribute("PacketSize", UintegerValue(packetSize));

  ApplicationContainer clientApps = client.Install(staNode);
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(simulationTime + 1));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  std::vector<std::string> errorModels = {"NistErrorRateModel", "YansErrorRateModel", "TableBasedErrorRateModel"};
  std::vector<std::string> mcsValues = {"HE_MCS0", "HE_MCS1", "HE_MCS2", "HE_MCS3", "HE_MCS4", "HE_MCS5", "HE_MCS6", "HE_MCS7",
                                        "HE_MCS8", "HE_MCS9", "HE_MCS10", "HE_MCS11"};

  for (const auto& errorModel : errorModels) {
    for (const auto& mcsValue : mcsValues) {
      std::string filename = errorModel + "_" + mcsValue + ".dat";
      std::ofstream outfile(filename);
      outfile << "# SNR FrameSuccessRate\n";

      for (double snr = startSNR; snr <= endSNR; snr += snrStep) {
        Config::Set("/NodeList/*/*/WifiNetDevice/Phy/SNR", DoubleValue(snr));
        wifiHelper.SetRemoteStationManager("ns3::HeWifiRemoteStationManager",
                                          "NonErpDataRate", StringValue(mcsValue),
                                          "HeGuardInterval", StringValue("LTF1_GI0_8"),
					  "ErrorRateModel", StringValue(errorModel));

	      phyHelper.SetErrorRateModelEnable (true);
	      for (uint32_t i = 0; i < apDevice.GetN(); ++i)
	        {
	          Ptr<NetDevice> nd = apDevice.Get (i);
	          Ptr<WifiNetDevice> wnd = nd->GetObject<WifiNetDevice> ();
	          Ptr<WifiPhy> phy = wnd->GetPhy ();

	          phy->SetErrorRateModelEnable (true);
	        }

	      for (uint32_t i = 0; i < staDevice.GetN(); ++i)
	        {
	          Ptr<NetDevice> nd = staDevice.Get (i);
	          Ptr<WifiNetDevice> wnd = nd->GetObject<WifiNetDevice> ();
	          Ptr<WifiPhy> phy = wnd->GetPhy ();
	          phy->SetErrorRateModelEnable (true);
	        }


        Simulator::Stop(Seconds(simulationTime));
        Simulator::Run();

        uint64_t totalPacketsSent = DynamicCast<UdpClient>(clientApps.Get(0))->GetTotalPacketsSent();
        uint64_t totalPacketsReceived = DynamicCast<UdpServer>(serverApps.Get(0))->GetTotalPacketsReceived();
        double frameSuccessRate = (double)totalPacketsReceived / totalPacketsSent;

        outfile << snr << " " << frameSuccessRate << "\n";
        Simulator::Destroy();
      }
      outfile.close();

      std::string gnuplotFilename = errorModel + "_" + mcsValue + ".plt";
      std::ofstream gnuplotFile(gnuplotFilename);
      gnuplotFile << "set terminal png\n";
      gnuplotFile << "set output '" << errorModel << "_" << mcsValue << ".png'\n";
      gnuplotFile << "set title '" << errorModel << " - " << mcsValue << "'\n";
      gnuplotFile << "set xlabel 'SNR (dB)'\n";
      gnuplotFile << "set ylabel 'Frame Success Rate'\n";
      gnuplotFile << "plot '" << filename << "' with lines title '" << errorModel << "'\n";
      gnuplotFile.close();
    }
  }

  return 0;
}