#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include "ns3/command-line.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool useUdp = true;
    std::string frequencyBand = "2.4";
    int channelWidth = 20;
    bool useShortGuardInterval = true;
    bool usePreambleDetection = false;
    double simulationTime = 10.0;
    double stepSize = 10.0;
    bool channelBonding = false;
    uint32_t maxPackets = 0;

    CommandLine cmd;
    cmd.AddValue("useUdp", "Use UDP traffic (default=true)", useUdp);
    cmd.AddValue("frequencyBand", "Frequency band (2.4 or 5.0, default=2.4)", frequencyBand);
    cmd.AddValue("channelWidth", "Channel width (default=20)", channelWidth);
    cmd.AddValue("useShortGuardInterval", "Use short guard interval (default=true)", useShortGuardInterval);
    cmd.AddValue("usePreambleDetection", "Use preamble detection (default=false)", usePreambleDetection);
    cmd.AddValue("simulationTime", "Simulation time (default=10.0)", simulationTime);
    cmd.AddValue("stepSize", "Distance step size (default=10.0)", stepSize);
    cmd.AddValue("channelBonding", "Enable channel bonding (default=false)", channelBonding);
    cmd.AddValue("maxPackets", "Max packets (default=0, unlimited)", maxPackets);
    cmd.Parse(argc, argv);

    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    NodeContainer staNodes;
    staNodes.Create(1);
    NodeContainer apNodes;
    apNodes.Create(1);

    YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();
    phyHelper.SetChannel(channelHelper.Create());

    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211n);

    WifiMacHelper macHelper;

    Ssid ssid = Ssid("ns3-80211n");
    macHelper.SetType("ns3::StaWifiMac",
                      "Ssid", SsidValue(ssid),
                      "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifiHelper.Install(phyHelper, macHelper, staNodes);

    macHelper.SetType("ns3::ApWifiMac",
                      "Ssid", SsidValue(ssid),
                      "BeaconInterval", TimeValue(MicroSeconds(102400)),
                      "QosSupported", BooleanValue(true));

    NetDeviceContainer apDevices = wifiHelper.Install(phyHelper, macHelper, apNodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                    "MinX", DoubleValue(0.0),
                                    "MinY", DoubleValue(0.0),
                                    "DeltaX", DoubleValue(stepSize),
                                    "DeltaY", DoubleValue(0.0),
                                    "GridWidth", UintegerValue(1),
                                    "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes);
    mobility.Install(apNodes);

    InternetStackHelper stackHelper;
    stackHelper.Install(staNodes);
    stackHelper.Install(apNodes);

    Ipv4AddressHelper addressHelper;
    addressHelper.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = addressHelper.Assign(staDevices);
    Ipv4InterfaceContainer apInterfaces = addressHelper.Assign(apDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Gnuplot2dDataset dataset[4][8];
    Gnuplot plot("throughput_vs_distance.png");
    plot.SetTitle("Throughput vs Distance for 802.11n MIMO");
    plot.SetLegend ("Distance (m)", "Throughput (Mbps)");

    for (int streams = 1; streams <= 4; ++streams) {
        for (int mcs = 0; mcs < 8; ++mcs) {
            std::stringstream ss;
            ss << "Streams=" << streams << ", MCS=" << mcs;
            dataset[streams-1][mcs].SetTitle(ss.str());
            dataset[streams-1][mcs].SetStyle(Gnuplot2dDataset::LINES_POINTS);
        }
    }

    for (double distance = stepSize; distance <= 100; distance += stepSize) {
        mobility.SetPosition(staNodes.Get(0), Vector(distance, 0, 0));

        for (int streams = 1; streams <= 4; ++streams) {
            for (int mcs = 0; mcs < 8; ++mcs) {
                Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/MpduDensity",
                            EnumValue(streams));
                Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/Htpc",
                            BooleanValue(true));
                Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/Mcs",
                            UintegerValue(mcs));

                uint16_t port = 9;
                ApplicationContainer app;
                if (useUdp) {
                  UdpServerHelper server(port);
                  app = server.Install(apNodes.Get(0));
                  app.Start(Seconds(0.0));
                  app.Stop(Seconds(simulationTime + 1));

                  UdpClientHelper client(apInterfaces.GetAddress(0), port);
                  client.SetAttribute("MaxPackets", UintegerValue(maxPackets));
                  client.SetAttribute("Interval", TimeValue(Time("0.00001")));
                  client.SetAttribute("PacketSize", UintegerValue(1472));
                  app = client.Install(staNodes.Get(0));

                } else {
                  uint16_t port = 9;
                  BulkSendHelper source("ns3::TcpSocketFactory",
                                          InetSocketAddress(apInterfaces.GetAddress(0), port));
                  source.SetAttribute("MaxBytes", UintegerValue(0));
                  app = source.Install(staNodes.Get(0));

                  PacketSinkHelper sink("ns3::TcpSocketFactory",
                                        InetSocketAddress(Ipv4Address::GetAny(), port));
                  app = sink.Install(apNodes.Get(0));

                }

                app.Start(Seconds(1.0));
                app.Stop(Seconds(simulationTime));
                FlowMonitorHelper flowmon;
                Ptr<FlowMonitor> monitor = flowmon.InstallAll();

                Simulator::Run(Seconds(simulationTime + 2));

                monitor->CheckForLostPackets();
                Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
                std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
                double throughput = 0.0;
                for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
                    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
                    if (t.sourceAddress == staInterfaces.GetAddress(0) && t.destinationAddress == apInterfaces.GetAddress(0)) {
                        throughput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
                        break;
                    }
                }
                dataset[streams-1][mcs].Add(distance, throughput);
                Simulator::Destroy();
            }
        }
    }

    for (int streams = 0; streams < 4; ++streams) {
        for (int mcs = 0; mcs < 8; ++mcs) {
            plot.AddDataset(dataset[streams][mcs]);
        }
    }

    plot.GenerateOutput();

    return 0;
}