#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/gnuplot.h"

#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiRateComparison");

int main(int argc, char* argv[]) {
    bool verbose = false;
    std::string rateControl = "ParfWifiManager";
    uint32_t rtsThreshold = 2347;
    std::string throughputFilename = "throughput.dat";
    std::string txPowerFilename = "txpower.dat";
    std::string animationFile = "wifi-animation.xml";
    double simulationTime = 10.0;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if set.", verbose);
    cmd.AddValue("rateControl", "Rate control algorithm: ParfWifiManager, AparfWifiManager, RrpaaWifiManager", rateControl);
    cmd.AddValue("rtsThreshold", "RTS/CTS threshold", rtsThreshold);
    cmd.AddValue("throughputFilename", "Throughput output filename", throughputFilename);
    cmd.AddValue("txPowerFilename", "Transmit power output filename", txPowerFilename);
    cmd.AddValue("animationFile", "NetAnim output filename", animationFile);
    cmd.AddValue("simulationTime", "Simulation time (seconds)", simulationTime);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("WifiRateComparison", LOG_LEVEL_INFO);
    }

    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(rtsThreshold));

    NodeContainer nodes;
    nodes.Create(2);

    NodeContainer apNode = NodeContainer(nodes.Get(0));
    NodeContainer staNode = NodeContainer(nodes.Get(1));

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
    wifi.SetRemoteStationManager(rateControl, "DataMode", StringValue("DsssRate11Mbps"));

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevice = wifi.Install(phy, mac, staNode);

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

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(staNode);

    Ptr<ConstantVelocityMobilityModel> staMob = staNode.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    staMob->SetVelocity(Vector3D(1, 0, 0));  // Move 1 meter per second along the x-axis.

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(apDevice);
    address.Assign(staDevice);

    UdpClientHelper client(interfaces.GetAddress(1), 9);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295));
    client.SetAttribute("Interval", TimeValue(Time("0.000001"))); //1 microsecond
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(apNode);
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));

    UdpServerHelper server(9);
    ApplicationContainer serverApps = server.Install(staNode);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AnimationInterface anim(animationFile);
    anim.SetConstantPosition(apNode.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(staNode.Get(0), 5.0, 0.0);

    // Configure Tracing
    Ptr<OutputStreamWrapper> throughputStream = Create<OutputStreamWrapper>(throughputFilename, std::ios::out);
    Ptr<OutputStreamWrapper> txPowerStream = Create<OutputStreamWrapper>(txPowerFilename, std::ios::out);

    Simulator::Schedule(Seconds(1.1), [&]() {
        *throughputStream->GetStream() << "Time(s) Distance(m) Throughput(Mbps)" << std::endl;
        *txPowerStream->GetStream() << "Time(s) Distance(m) TxPower(dBm)" << std::endl;
    });

    Simulator::Schedule(Seconds(1.1), [&]() {
       Simulator::Schedule(Seconds(0.1), [&, i = 0]() mutable {
            if (i < (simulationTime - 1.0) * 10.0) {
                double time = 1.1 + i * 0.1;
                double distance = 5.0 + (time - 1.0);

                Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
                std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
                double throughput = 0;
                for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
                {
                    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
                    if (t.sourceAddress == interfaces.GetAddress(0) && t.destinationAddress == interfaces.GetAddress(1))
                    {
                        throughput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
                        break;
                    }
                }

                *throughputStream->GetStream() << time << " " << distance << " " << throughput << std::endl;

                // Get transmit power from the phy layer
                double txPower = 0.0;
                Ptr<WifiPhy> phyPtr = DynamicCast<WifiPhy>(staDevice.Get(0)->GetPhy());
                if (phyPtr) {
                   txPower = phyPtr->GetLastTxPowerDbm();
                }

                *txPowerStream->GetStream() << time << " " << distance << " " << txPower << std::endl;

                Simulator::Schedule(Seconds(0.1), [=]() mutable { i++; });
                if (i < (simulationTime - 1.0) * 10.0) {
                    Simulator::ScheduleNow(std::bind(Simulator::GetCurrentContext()->GetFunction()));
                }
            }
        });
    });

    Simulator::Stop(Seconds(simulationTime));

    Simulator::Run();

    monitor->SerializeToXmlFile("wifi_rate_comparison.xml", true, true);

    Simulator::Destroy();

    return 0;
}