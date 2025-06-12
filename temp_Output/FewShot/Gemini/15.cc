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

NS_LOG_COMPONENT_DEFINE("WifiMimoSimulation");

int main(int argc, char *argv[]) {
    bool verbose = false;
    uint32_t nPackets = 100;
    double interval = 0.001;
    double simulationTime = 10;
    std::string dataRate = "50Mbps";
    double distanceStep = 10;
    std::string trafficType = "UDP";
    std::string frequencyBand = "2.4GHz";
    bool shortGuardInterval = true;
    bool channelBonding = false;
    bool preambleDetection = false;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("nPackets", "Number of packets generated", nPackets);
    cmd.AddValue("interval", "Interval between packets in seconds", interval);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("dataRate", "Data rate", dataRate);
    cmd.AddValue("distanceStep", "Distance step between nodes", distanceStep);
    cmd.AddValue("trafficType", "Traffic type (UDP or TCP)", trafficType);
    cmd.AddValue("frequencyBand", "Frequency band (2.4GHz or 5.0GHz)", frequencyBand);
    cmd.AddValue("shortGuardInterval", "Enable short guard interval", shortGuardInterval);
    cmd.AddValue("channelBonding", "Enable channel bonding", channelBonding);
    cmd.AddValue("preambleDetection", "Enable preamble detection", preambleDetection);

    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNode;
    staNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();

    Ssid ssid = Ssid("ns-3-ssid");

    WifiMacHelper apmac;
    apmac.SetType("ns3::ApWifiMac",
        "Ssid", SsidValue(ssid),
        "BeaconInterval", TimeValue (MilliSeconds (100)));

    NetDeviceContainer apDevice = wifi.Install(phy, apmac, apNode);

    WifiMacHelper stamac;
    stamac.SetType("ns3::StaWifiMac",
                 "Ssid", SsidValue(ssid),
                 "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevice = wifi.Install(phy, stamac, staNode);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
        "MinX", DoubleValue(0.0),
        "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(distanceStep),
        "DeltaY", DoubleValue(0.0),
        "GridWidth", UintegerValue(1),
        "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNode);

    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 50000;

    std::string mcsValues[16] = {"VhtMcs0", "VhtMcs1", "VhtMcs2", "VhtMcs3", "VhtMcs4", "VhtMcs5", "VhtMcs6", "VhtMcs7",
                                 "VhtMcs8", "VhtMcs9", "VhtMcs10", "VhtMcs11", "VhtMcs12", "VhtMcs13", "VhtMcs14", "VhtMcs15"};

    std::map<int, std::vector<double>> throughputData;

    for (int numStreams = 1; numStreams <= 4; ++numStreams) {
        throughputData[numStreams] = std::vector<double>(16, 0.0);
        for (int mcsIndex = 0; mcsIndex < 16; ++mcsIndex) {
            std::string mcs = mcsValues[mcsIndex];

            Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue(channelBonding ? 40 : 20));
            Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ShortGuardIntervalSupported", BooleanValue(shortGuardInterval));
            Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PreambleDetection", BooleanValue(preambleDetection));

            std::string rate;
            if(numStreams == 1){
                rate = "HtMcs" + std::to_string(mcsIndex);
            }
            else if(numStreams == 2){
              rate = "VhtNss2Mcs" + std::to_string(mcsIndex);
            }
            else if(numStreams == 3){
              rate = "VhtNss3Mcs" + std::to_string(mcsIndex);
            }
            else{
              rate = "VhtNss4Mcs" + std::to_string(mcsIndex);
            }
            
            Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/SupportedTxMcs", StringValue (rate));
            Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/SupportedRxMcs", StringValue (rate));
            
            

            ApplicationContainer clientApps;
            ApplicationContainer serverApps;

            if (trafficType == "UDP") {
                UdpServerHelper server(port);
                serverApps = server.Install(apNode.Get(0));
                serverApps.Start(Seconds(1.0));
                serverApps.Stop(Seconds(simulationTime));

                UdpClientHelper client(apInterface.GetAddress(0), port);
                client.SetAttribute("MaxPackets", UintegerValue(nPackets));
                client.SetAttribute("Interval", TimeValue(Seconds(interval)));
                client.SetAttribute("PacketSize", UintegerValue(1024));
                clientApps = client.Install(staNode.Get(0));
                clientApps.Start(Seconds(2.0));
                clientApps.Stop(Seconds(simulationTime));
            } else {
                BulkSendHelper client(apInterface.GetAddress(0), port);
                client.SetAttribute("MaxBytes", UintegerValue(0));
                clientApps = client.Install(staNode.Get(0));
                clientApps.Start(Seconds(2.0));
                clientApps.Stop(Seconds(simulationTime));

                PacketSinkHelper sink(port);
                serverApps = sink.Install(apNode.Get(0));
                serverApps.Start(Seconds(1.0));
                serverApps.Stop(Seconds(simulationTime));
            }

            FlowMonitorHelper flowmon;
            Ptr<FlowMonitor> monitor = flowmon.InstallAll();

            Simulator::Stop(Seconds(simulationTime));
            Simulator::Run();

            monitor->CheckForLostPackets();
            Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
            std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

            double totalBytes = 0;
            double totalTime = 0;

            for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
                Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
                if (t.sourceAddress == staInterface.GetAddress(0) && t.destinationAddress == apInterface.GetAddress(0)) {
                    totalBytes = i->second.bytesReceived * 8.0;
                    totalTime = i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds();
                    break;
                }
            }

            double throughput = (totalBytes / totalTime) / 1000000.0;
            throughputData[numStreams][mcsIndex] = throughput;

            Simulator::Destroy();
        }
    }

    std::string fileNameWithNoExtension = "wifi-mimo-throughput";
    std::string graphicsFileName = fileNameWithNoExtension + ".png";
    std::string plotFileName = fileNameWithNoExtension + ".plt";
    std::string dataFileName = fileNameWithNoExtension + ".dat";

    Gnuplot gnuplot(graphicsFileName);
    gnuplot.SetTitle("802.11n MIMO Throughput vs MCS");
    gnuplot.SetLegend("MCS Index", "Throughput (Mbps)");

    std::ofstream out(dataFileName.c_str());
    out << "# MCS Index\t";
    for (int numStreams = 1; numStreams <= 4; ++numStreams) {
        out << "Streams " << numStreams << "\t";
    }
    out << std::endl;

    for (int mcsIndex = 0; mcsIndex < 16; ++mcsIndex) {
        out << mcsIndex << "\t";
        for (int numStreams = 1; numStreams <= 4; ++numStreams) {
            out << throughputData[numStreams][mcsIndex] << "\t";
        }
        out << std::endl;
    }
    out.close();

    gnuplot.AddDataset(dataFileName, "index 0 using 1:2 title 'Streams 1' with linespoints", "index 0 using 1:3 title 'Streams 2' with linespoints", "index 0 using 1:4 title 'Streams 3' with linespoints", "index 0 using 1:5 title 'Streams 4' with linespoints");

    gnuplot.GenerateOutput(plotFileName);

    return 0;
}