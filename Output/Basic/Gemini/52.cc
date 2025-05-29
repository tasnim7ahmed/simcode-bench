#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/command-line.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    double simulationTime = 10.0;
    std::string phyMode = "HtMcs7";
    double slotTime = 9e-6;
    double sifs = 16e-6;
    double pifs = 25e-6;

    CommandLine cmd;
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
    cmd.AddValue("slotTime", "Slot time in seconds", slotTime);
    cmd.AddValue("sifs", "SIFS in seconds", sifs);
    cmd.AddValue("pifs", "PIFS in seconds", pifs);
    cmd.Parse(argc, argv);

    NodeContainer staNodes;
    staNodes.Create(1);

    NodeContainer apNode;
    apNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);

    WifiMacHelper mac;

    Ssid ssid = Ssid("ns-3-ssid");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, staNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(0.1)));

    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, apNode);

    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Slot", TimeValue (Seconds (slotTime)));
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Sifs", TimeValue (Seconds (sifs)));
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Pifs", TimeValue (Seconds (pifs)));

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
    mobility.Install(staNodes);

    InternetStackHelper internet;
    internet.Install(apNode);
    internet.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staNodeInterface;
    staNodeInterface = address.Assign(staDevices);
    Ipv4InterfaceContainer apNodeInterface;
    apNodeInterface = address.Assign(apDevices);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(staNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));

    UdpEchoClientHelper echoClient(staNodeInterface.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(4294967295));
    echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(apNode.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));

    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalBytes = 0.0;
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if (t.destinationAddress == staNodeInterface.GetAddress(0))
        {
            totalBytes += i->second.rxBytes * 8.0;
        }
    }

    double throughput = totalBytes / (simulationTime * 1e6);

    std::cout << "Throughput: " << throughput << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}