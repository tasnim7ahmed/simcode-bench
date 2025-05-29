#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/command-line.h"
#include "ns3/gnuplot.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/nist-error-rate-model.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/wifi-mac-header.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiErrorRateModels");

int main(int argc, char *argv[]) {
  bool verbose = false;
  uint32_t packetSize = 1000;
  double simTime = 1;
  double startSNR = 5;
  double endSNR = 25;
  double snrStep = 1;

  CommandLine cmd(__FILE__);
  cmd.AddValue("verbose", "Tell application to log if set.", verbose);
  cmd.AddValue("packetSize", "Size of packet generated", packetSize);
  cmd.AddValue("simTime", "Simulation time in seconds", simTime);
  cmd.AddValue("startSNR", "Start SNR value", startSNR);
  cmd.AddValue("endSNR", "End SNR value", endSNR);
  cmd.AddValue("snrStep", "SNR step value", snrStep);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("WifiErrorRateModels", LOG_LEVEL_INFO);
  }

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  NistErrorRateModel nistErrorRateModel;
  phy.SetErrorRateModel("ns3::NistErrorRateModel");

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, nodes.Get(1));

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, nodes.Get(0));

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0));
  positionAlloc->Add(Vector(5.0, 0.0, 0.0));
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  for (int mcs = 0; mcs <= 7; ++mcs) {
    std::ofstream nistDataFile;
    std::ofstream yansDataFile;
    std::ofstream tableDataFile;

    nistDataFile.open("nist_mcs" + std::to_string(mcs) + ".dat");
    yansDataFile.open("yans_mcs" + std::to_string(mcs) + ".dat");
    tableDataFile.open("table_mcs" + std::to_string(mcs) + ".dat");

    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode",
                                 StringValue("HtMcs" + std::to_string(mcs)),
                                 "ControlMode",
                                 StringValue("HtMcs" + std::to_string(mcs)));

    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue(20));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/GuardInterval", TimeValue(Seconds(0.8)));
    
    for (double snr = startSNR; snr <= endSNR; snr += snrStep) {
      double nistSuccessRateSum = 0;
      double yansSuccessRateSum = 0;
      double tableSuccessRateSum = 0;

      for (int run = 0; run < 10; ++run) {
        Simulator::Destroy();
        phy.Set("RxNoiseFigure", DoubleValue(5));
        phy.Set("TxPowerStart", DoubleValue(snr));
        phy.Set("TxPowerEnd", DoubleValue(snr));

        Packet::EnablePrinting();
        uint32_t numPacketsReceivedNist = 0;
        uint32_t numPacketsReceivedYans = 0;
        uint32_t numPacketsReceivedTable = 0;
        
        NistErrorRateModel::SetSnr(snr);

        Ptr<UdpClient> client = CreateObject<UdpClient>();
        nodes.Get(1)->AddApplication(client);
        client->SetAttribute("RemoteAddress", AddressValue(apDevices.Get(0)->GetAddress()));
        client->SetAttribute("RemotePort", UintegerValue(9));
        client->SetAttribute("PacketSize", UintegerValue(packetSize));
        client->SetAttribute("MaxPackets", UintegerValue(100));
        client->SetStartTime(Seconds(0.0));
        client->SetStopTime(Seconds(simTime));

        Ptr<UdpServer> server = CreateObject<UdpServer>();
        nodes.Get(0)->AddApplication(server);
        server->SetAttribute("LocalPort", UintegerValue(9));
        server->SetStartTime(Seconds(0.0));
        server->SetStopTime(Seconds(simTime));

        Simulator::Stop(Seconds(simTime));

        Config::ConnectWithoutContext("/NodeList/0/ApplicationList/0/Rx", MakeCallback(&UdpServer::HandleRx));
        
        phy.SetErrorRateModel("ns3::NistErrorRateModel");
        Simulator::Run();
        numPacketsReceivedNist = server->GetReceived();
        double nistSuccessRate = (double)numPacketsReceivedNist / 100.0;
        nistSuccessRateSum += nistSuccessRate;

        Simulator::Destroy();
        phy.Set("RxNoiseFigure", DoubleValue(5));
        phy.Set("TxPowerStart", DoubleValue(snr));
        phy.Set("TxPowerEnd", DoubleValue(snr));
        Ptr<UdpClient> client2 = CreateObject<UdpClient>();
        nodes.Get(1)->AddApplication(client2);
        client2->SetAttribute("RemoteAddress", AddressValue(apDevices.Get(0)->GetAddress()));
        client2->SetAttribute("RemotePort", UintegerValue(9));
        client2->SetAttribute("PacketSize", UintegerValue(packetSize));
        client2->SetAttribute("MaxPackets", UintegerValue(100));
        client2->SetStartTime(Seconds(0.0));
        client2->SetStopTime(Seconds(simTime));

        Ptr<UdpServer> server2 = CreateObject<UdpServer>();
        nodes.Get(0)->AddApplication(server2);
        server2->SetAttribute("LocalPort", UintegerValue(9));
        server2->SetStartTime(Seconds(0.0));
        server2->SetStopTime(Seconds(simTime));

        Simulator::Stop(Seconds(simTime));

        Config::ConnectWithoutContext("/NodeList/0/ApplicationList/0/Rx", MakeCallback(&UdpServer::HandleRx));
        phy.SetErrorRateModel("ns3::YansErrorRateModel");
        Simulator::Run();
        numPacketsReceivedYans = server2->GetReceived();
        double yansSuccessRate = (double)numPacketsReceivedYans / 100.0;
        yansSuccessRateSum += yansSuccessRate;
        
        Simulator::Destroy();
         phy.Set("RxNoiseFigure", DoubleValue(5));
        phy.Set("TxPowerStart", DoubleValue(snr));
        phy.Set("TxPowerEnd", DoubleValue(snr));
        Ptr<UdpClient> client3 = CreateObject<UdpClient>();
        nodes.Get(1)->AddApplication(client3);
        client3->SetAttribute("RemoteAddress", AddressValue(apDevices.Get(0)->GetAddress()));
        client3->SetAttribute("RemotePort", UintegerValue(9));
        client3->SetAttribute("PacketSize", UintegerValue(packetSize));
        client3->SetAttribute("MaxPackets", UintegerValue(100));
        client3->SetStartTime(Seconds(0.0));
        client3->SetStopTime(Seconds(simTime));

        Ptr<UdpServer> server3 = CreateObject<UdpServer>();
        nodes.Get(0)->AddApplication(server3);
        server3->SetAttribute("LocalPort", UintegerValue(9));
        server3->SetStartTime(Seconds(0.0));
        server3->SetStopTime(Seconds(simTime));

        Simulator::Stop(Seconds(simTime));

        Config::ConnectWithoutContext("/NodeList/0/ApplicationList/0/Rx", MakeCallback(&UdpServer::HandleRx));
        phy.SetErrorRateModel("ns3::TableBasedErrorRateModel");
        Simulator::Run();
        numPacketsReceivedTable = server3->GetReceived();
        double tableSuccessRate = (double)numPacketsReceivedTable / 100.0;
        tableSuccessRateSum += tableSuccessRate;
      }
      
      double nistSuccessRateAvg = nistSuccessRateSum / 10.0;
      double yansSuccessRateAvg = yansSuccessRateSum / 10.0;
      double tableSuccessRateAvg = tableSuccessRateSum / 10.0;

      nistDataFile << snr << " " << nistSuccessRateAvg << std::endl;
      yansDataFile << snr << " " << yansSuccessRateAvg << std::endl;
      tableDataFile << snr << " " << tableSuccessRateAvg << std::endl;
    }

    nistDataFile.close();
    yansDataFile.close();
    tableDataFile.close();

    Gnuplot plot;
    plot.SetTitle("Frame Success Rate vs SNR (MCS " + std::to_string(mcs) + ")");
    plot.SetLegend("NIST", "YANS", "Table");
    plot.AddDataset("NIST", "nist_mcs" + std::to_string(mcs) + ".dat", Gnuplot::STYLE_LINES);
    plot.AddDataset("YANS", "yans_mcs" + std::to_string(mcs) + ".dat", Gnuplot::STYLE_LINES);
    plot.AddDataset("Table", "table_mcs" + std::to_string(mcs) + ".dat", Gnuplot::STYLE_LINES);
    plot.SetXAxis("SNR (dB)");
    plot.SetYAxis("Frame Success Rate");
    plot.GenerateOutput("frame_success_rate_mcs" + std::to_string(mcs) + ".plt");
  }

  Simulator::Destroy();
  return 0;
}