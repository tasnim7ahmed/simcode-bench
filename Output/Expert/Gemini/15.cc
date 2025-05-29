#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool verbose = false;
    std::string protocol = "Udp";
    std::string frequency = "2.4";
    bool channelBonding = true;
    bool shortGuardInterval = true;
    bool preambleDetection = true;
    double simulationTime = 10;
    double stepSize = 10;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if set.", verbose);
    cmd.AddValue("protocol", "Protocol to use: Udp or Tcp", protocol);
    cmd.AddValue("frequency", "Frequency band (2.4 or 5.0 GHz)", frequency);
    cmd.AddValue("channelBonding", "Enable channel bonding", channelBonding);
    cmd.AddValue("shortGuardInterval", "Enable short guard interval", shortGuardInterval);
    cmd.AddValue("preambleDetection", "Enable preamble detection", preambleDetection);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("stepSize", "Distance step size", stepSize);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    Config::SetDefault ("ns3::WifiMacQueue::MaxSize", StringValue ("64"));
    Config::SetDefault ("ns3::WifiMacQueue::Mode", EnumValue (WifiMacQueue::ADAPTIVE));

    NodeContainer staNodes;
    staNodes.Create(1);
    NodeContainer apNodes;
    apNodes.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-80211n");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(0.05)));

    NetDeviceContainer apDevices = wifi.Install(phy, mac, apNodes);

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

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes);

    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    ApplicationContainer clientApps;
    ApplicationContainer serverApps;

    if (protocol == "Udp") {
        UdpServerHelper echoServer(port);
        serverApps = echoServer.Install(apNodes.Get(0));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(simulationTime + 1));

        UdpClientHelper echoClient(apInterfaces.GetAddress(0), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(4294967295));
        echoClient.SetAttribute("Interval", TimeValue(NanoSeconds(1)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1472));
        clientApps = echoClient.Install(staNodes.Get(0));
    } else if (protocol == "Tcp") {
        TcpServerHelper echoServer(port);
        serverApps = echoServer.Install(apNodes.Get(0));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(simulationTime + 1));

        TcpClientHelper echoClient(apInterfaces.GetAddress(0), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(4294967295));
        echoClient.SetAttribute("Interval", TimeValue(NanoSeconds(1)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1472));
        clientApps = echoClient.Install(staNodes.Get(0));
    } else {
        std::cerr << "Invalid protocol specified.  Choose Udp or Tcp." << std::endl;
        return 1;
    }

    std::ofstream throughputFile ("throughput.dat");
    throughputFile << "# Distance Throughput (Mbps)" << std::endl;

    for (double distance = 10; distance <= 100; distance += stepSize) {
        for (int numStreams = 1; numStreams <= 4; ++numStreams) {
            for (int mcs = 0; mcs <= 23; ++mcs) {
                std::stringstream ss;
                ss << "HtMcs" << mcs;
                std::string mcsString = ss.str();

                Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (channelBonding ? 20 : 10));
                Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ShortGuardIntervalSupported", BooleanValue (shortGuardInterval));
                Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PreambleDetection", BooleanValue (preambleDetection));

                wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                            "DataMode", StringValue("HtMcs" + std::to_string(mcs)),
                                            "ControlMode", StringValue("HtMcs" + std::to_string(mcs)));

                clientApps.Start(Seconds(2.0));
                clientApps.Stop(Seconds(simulationTime));

                ((ConstantPositionMobilityModel *) staNodes.Get(0)->GetObject<MobilityModel>())->SetPosition(Vector(distance, 0, 0));

                FlowMonitorHelper flowmon;
                Ptr<FlowMonitor> monitor = flowmon.InstallAll();

                Simulator::Stop(Seconds(simulationTime + 1));
                Simulator::Run();

                monitor->CheckForLostPackets();
                Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
                FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
                double throughput = 0.0;

                for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
                    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
                    if (t.destinationAddress == apInterfaces.GetAddress(0)) {
                        throughput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
                        break;
                    }
                }

                throughputFile << distance << " " << numStreams << " " << mcs << " " << throughput << std::endl;

                monitor->SerializeToXmlFile("flowmon.xml", false, true);

                Simulator::Destroy();
            }
        }
    }

    throughputFile.close();

    Gnuplot plot("throughput.eps");
    plot.SetTitle("Throughput vs. Distance");
    plot.SetTerminal("postscript eps color");
    plot.SetLegend("Distance (m)", "Throughput (Mbps)");

    Gnuplot2dDataset dataset;
    dataset.SetTitle("Throughput");
    dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
    dataset.SetFilename("throughput.dat");
    dataset.SetYColumn(3);
    dataset.SetXColumn(0);

    plot.AddDataset(dataset);

    std::ofstream plotFile("throughput.plt");
    plot.GenerateOutput(plotFile);
    plotFile.close();

    return 0;
}