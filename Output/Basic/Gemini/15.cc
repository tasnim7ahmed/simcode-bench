#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMimoSimulation");

int main(int argc, char *argv[]) {
    bool verbose = false;
    uint32_t packetSize = 1472;
    std::string dataRate = "50Mbps";
    std::string phyMode = "HtMcs7";
    double simulationTime = 10;
    double distanceStep = 10;
    bool enableChannelBonding = true;
    std::string transportProtocol = "Udp";
    double startDistance = 10;
    double endDistance = 100;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if true", verbose);
    cmd.AddValue("packetSize", "Size of packets generated", packetSize);
    cmd.AddValue("dataRate", "Rate at which data is generated", dataRate);
    cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("distanceStep", "Distance step between nodes", distanceStep);
    cmd.AddValue("enableChannelBonding", "Enable channel bonding", enableChannelBonding);
    cmd.AddValue("transportProtocol", "Transport protocol (Udp or Tcp)", transportProtocol);
    cmd.AddValue("startDistance", "Start distance between nodes", startDistance);
    cmd.AddValue("endDistance", "End distance between nodes", endDistance);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("WifiMimoSimulation", LOG_LEVEL_INFO);
    }

    if (transportProtocol != "Udp" && transportProtocol != "Tcp") {
        std::cerr << "Invalid transport protocol. Must be Udp or Tcp." << std::endl;
        return 1;
    }

    NodeContainer staNodes;
    staNodes.Create(1);

    NodeContainer apNodes;
    apNodes.Create(1);

    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211n);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    wifiPhy.Set("ShortGuardIntervalSupported", BooleanValue(true));

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns-3-ssid");
    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifiHelper.Install(wifiPhy, wifiMac, staNodes);

    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid),
                    "BeaconInterval", TimeValue(Seconds(0.050)));

    NetDeviceContainer apDevices = wifiHelper.Install(wifiPhy, wifiMac, apNodes);

    if (enableChannelBonding) {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue(40));
    } else {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue(20));
    }

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
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staNodeInterface;
    Ipv4InterfaceContainer apNodeInterface;
    apNodeInterface = address.Assign(apDevices);
    address.Assign(staDevices);
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;

    std::vector<std::string> mcsValues = {"HtMcs0", "HtMcs1", "HtMcs2", "HtMcs3", "HtMcs4", "HtMcs5", "HtMcs6", "HtMcs7"};

    Gnuplot2dDataset dataset[4][8];
    std::stringstream titleStream;

    for (int numStreams = 1; numStreams <= 4; ++numStreams) {
        for (size_t i = 0; i < mcsValues.size(); ++i) {
            std::string currentPhyMode = "HtMcs" + std::to_string(i);
            wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                                "DataMode", StringValue(currentPhyMode),
                                                "ControlMode", StringValue(currentPhyMode));

            for (double distance = startDistance; distance <= endDistance; distance += distanceStep) {
                Ptr<MobilityModel> staMobility = staNodes.Get(0)->GetObject<MobilityModel>();
                staMobility->SetPosition(Vector(distance, 0, 0));

                ApplicationContainer app;
                if (transportProtocol == "Udp") {
                    UdpClientHelper client(apNodeInterface.GetAddress(0), port);
                    client.SetAttribute("PacketSize", UintegerValue(packetSize));
                    client.SetAttribute("Interval", TimeValue(Seconds(0.0001)));
                    app = client.Install(staNodes.Get(0));

                    UdpServerHelper server(port);
                    app.Add(server.Install(apNodes.Get(0)));
                } else {
                    BulkSendHelper source("ns3::TcpSocketFactory",
                                           InetSocketAddress(apNodeInterface.GetAddress(0), port));
                    source.SetAttribute("SendSize", UintegerValue(packetSize));
                    app = source.Install(staNodes.Get(0));
                    PacketSinkHelper sink("ns3::TcpSocketFactory",
                                           InetSocketAddress(Ipv4Address::GetAny(), port));
                    app.Add(sink.Install(apNodes.Get(0)));
                }

                app.Start(Seconds(0.0));
                app.Stop(Seconds(simulationTime));

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
                    if (t.sourceAddress == staNodeInterface.GetAddress(0) && t.destinationAddress == apNodeInterface.GetAddress(0)) {
                        throughput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
                        break;
                    }
                }

                dataset[numStreams - 1][i].Add(distance, throughput);
                Simulator::Destroy();
            }
        }
    }

    std::string fileNameWithSuffix = "wifi-mimo-throughput.plt";
    std::string graphicsFileNameWithSuffix = "wifi-mimo-throughput.png";

    Gnuplot plot(fileNameWithSuffix);

    plot.SetTerminal("png size 1024,768");
    plot.SetLegend("lower right");
    plot.SetTitle("802.11n MIMO Throughput vs. Distance");
    plot.Setxlabel("Distance (m)");
    plot.Setylabel("Throughput (Mbps)");
    plot.AppendExtra("set xrange [0:110]");
    plot.AppendExtra("set yrange [0:*]");

    std::vector<std::string> streamLabels = {"1 Stream", "2 Streams", "3 Streams", "4 Streams"};
    std::vector<std::string> mcsLabels = {"MCS 0", "MCS 1", "MCS 2", "MCS 3", "MCS 4", "MCS 5", "MCS 6", "MCS 7"};

    for (int numStreams = 0; numStreams < 4; ++numStreams) {
        for (size_t i = 0; i < mcsValues.size(); ++i) {
            titleStream.str("");
            titleStream << streamLabels[numStreams] << ", " << mcsLabels[i];
            dataset[numStreams][i].SetTitle(titleStream.str());
            plot.AddDataset(dataset[numStreams][i]);
        }
    }

    plot.GenerateOutput(graphicsFileNameWithSuffix);

    return 0;
}