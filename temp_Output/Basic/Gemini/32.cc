#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

using namespace ns3;

int main(int argc, char *argv[]) {

  bool verbose = false;
  uint32_t packetSize = 1472; // bytes
  double simulationTime = 10; // seconds
  double snrStart = 0;        // dB
  double snrEnd = 30;         // dB
  double snrStep = 1;         // dB

  CommandLine cmd;
  cmd.AddValue("packetSize", "Size of packet generated. Default 1472", packetSize);
  cmd.AddValue("simulationTime", "Simulation time in seconds. Default 10", simulationTime);
  cmd.AddValue("snrStart", "Start SNR in dB. Default 0", snrStart);
  cmd.AddValue("snrEnd", "End SNR in dB. Default 30", snrEnd);
  cmd.AddValue("snrStep", "SNR step in dB. Default 1", snrStep);
  cmd.AddValue("verbose", "Tell application to log if all packets are received", verbose);

  cmd.Parse(argc, argv);

  std::vector<std::string> errorRateModelNames = {"NistErrorRateModel", "YansErrorRateModel", "TableBasedErrorRateModel"};

  for (const std::string &errorRateModelName : errorRateModelNames) {

    for (int mcs = 0; mcs <= 11; ++mcs) {

      std::ofstream outputFile;
      std::string filename = "he_" + errorRateModelName + "_mcs" + std::to_string(mcs) + ".dat";
      outputFile.open(filename);
      outputFile << "# SNR FrameSuccessRate" << std::endl;

      for (double snr = snrStart; snr <= snrEnd; snr += snrStep) {

        NodeContainer nodes;
        nodes.Create(2);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211ax);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy;
        phy.SetChannel(channel.Create());
        phy.Set("TxPowerStart", DoubleValue(20));
        phy.Set("TxPowerEnd", DoubleValue(20));

        if (errorRateModelName == "NistErrorRateModel") {
          wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                        "DataMode", StringValue("HE_MCS" + std::to_string(mcs)),
                                        "ControlMode", StringValue("HE_MCS0"));
          phy.SetErrorRateModel("ns3::NistErrorRateModel");
        } else if (errorRateModelName == "YansErrorRateModel") {
          wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                        "DataMode", StringValue("HE_MCS" + std::to_string(mcs)),
                                        "ControlMode", StringValue("HE_MCS0"));
          phy.SetErrorRateModel("ns3::YansErrorRateModel");
        } else if (errorRateModelName == "TableBasedErrorRateModel") {
            wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                            "DataMode", StringValue("HE_MCS" + std::to_string(mcs)),
                                            "ControlMode", StringValue("HE_MCS0"));

            phy.SetErrorRateModel("ns3::TableBasedErrorRateModel",
                                   "SpectrumModel", ObjectValue(CreateObject<SpectrumWifiPhy>()->GetSpectrumModel()),
                                   "ChannelWidth", UintegerValue(80)); // Assume 80 MHz channel for HE. Important to set.
        } else {
          std::cerr << "Unknown error rate model: " << errorRateModelName << std::endl;
          return 1;
        }

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns-3-ssid");
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

        NetDeviceContainer staDevices;
        staDevices = wifi.Install(phy, mac, nodes.Get(0));

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "BeaconGeneration", BooleanValue(true));

        NetDeviceContainer apDevices;
        apDevices = wifi.Install(phy, mac, nodes.Get(1));

        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0), "DeltaX", DoubleValue(5.0),
                                      "DeltaY", DoubleValue(10.0), "GridWidth", UintegerValue(3), "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(nodes);

        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(staDevices);
        address.Assign(apDevices);

        uint16_t port = 9;
        UdpClientHelper client(interfaces.GetAddress(0), port);
        client.SetAttribute("MaxPackets", UintegerValue(400000));
        client.SetAttribute("Interval", TimeValue(Seconds(0.00001)));
        client.SetAttribute("PacketSize", UintegerValue(packetSize));

        ApplicationContainer clientApps = client.Install(nodes.Get(1));
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(simulationTime + 1));

        UdpServerHelper server(port);
        ApplicationContainer serverApps = server.Install(nodes.Get(0));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(simulationTime + 1));

        Ptr<OutputStreamWrapper> stream = Create<OutputStreamWrapper>("wifi-simple-interference.pcap", std::ios::out);
        phy.EnablePcap("wifi-simple-interference", apDevices, false, true);

        // Add a device noise model
        Ptr<ConstantGainPropagationLossModel> propagationLossModel = CreateObject<ConstantGainPropagationLossModel>();
        Ptr<SpectrumPropagationLossModel> loss = CreateObject<SpectrumPropagationLossModel>();
        loss->AddPropagationLossModel(propagationLossModel);

        // Add spectrum channel
        Ptr<SingleModelSpectrumPropagationLossModel> propagationLoss = CreateObject<SingleModelSpectrumPropagationLossModel>();
        propagationLoss->SetAttribute("PropagationLossModel", PointerValue(loss));

        // Add propagation delay model
        Ptr<ConstantSpeedPropagationDelayModel> delay = CreateObject<ConstantSpeedPropagationDelayModel>();
        Ptr<SpectrumPropagationDelayModel> propagationDelay = CreateObject<SpectrumPropagationDelayModel>();
        propagationDelay->SetAttribute("PropagationDelayModel", PointerValue(delay));

        Ptr<SpectrumChannel> channelModel = CreateObject<SpectrumChannel>();
        channelModel->AddPropagationLossModel(propagationLoss);
        channelModel->SetPropagationDelayModel(propagationDelay);

        phy.SetChannel(channelModel);

        // Set SNR value
        phy.Set("RxNoiseFigure", DoubleValue(5));
        phy.Set("ThermalNoise", DoubleValue(-95));
        phy.Set("CcaEdThreshold", DoubleValue(-92));

        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/Sinr", DoubleValue(snr));
        Simulator::Stop(Seconds(simulationTime + 1));
        Simulator::Run();

        uint32_t totalPacketsSent = DynamicCast<UdpClient>(clientApps.Get(0))->GetTotalPacketsSent();
        uint32_t totalPacketsReceived = DynamicCast<UdpServer>(serverApps.Get(0))->GetReceived();
        double frameSuccessRate = (double)totalPacketsReceived / (double)totalPacketsSent;

        outputFile << snr << " " << frameSuccessRate << std::endl;

        Simulator::Destroy();
      }
      outputFile.close();

      std::cout << "Gnuplot data file generated: " << filename << std::endl;
    }
  }

  return 0;
}