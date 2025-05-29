#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/on-off-application.h"
#include "ns3/tcp-client-application.h"
#include "ns3/tcp-server-application.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiPerformance");

int main(int argc, char *argv[]) {
    bool verbose = false;
    bool tracing = false;
    std::string protocol = "Udp";
    double simulationTime = 10.0;
    uint32_t payloadSize = 1472;
    bool enableErpProtection = false;
    bool useShortPreamble = false;
    bool useShortSlotTime = false;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.AddValue("protocol", "Transport protocol to use: Tcp or Udp", protocol);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("erpProtection", "Enable ERP protection", enableErpProtection);
    cmd.AddValue("shortPreamble", "Use short preamble", useShortPreamble);
    cmd.AddValue("shortSlotTime", "Use short slot time", useShortSlotTime);

    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
        LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
        LogComponentEnable("TcpClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("TcpServerApplication", LOG_LEVEL_INFO);
    }

    Time::SetResolution(Time::NS);

    NodeContainer apNode;
    apNode.Create(1);

    NodeContainer staNodes;
    staNodes.Create(3);

    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211g);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.Set("RxGain", DoubleValue(10));
    wifiPhy.Set("TxGain", DoubleValue(10));
    wifiPhy.Set("EnergyDetectionThreshold", DoubleValue(-96));
    wifiPhy.Set("CcaEdThreshold", DoubleValue(-93));

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns-3-ssid");

    // Configure AP
    wifiMac.SetType("ns3::ApWifiMac",
                   "Ssid", SsidValue(ssid),
                   "BeaconInterval", TimeValue(MilliSeconds(100)));

    NetDeviceContainer apDevice = wifiHelper.Install(wifiPhy, wifiMac, apNode);

    // Configure stations
    wifiMac.SetType("ns3::StaWifiMac",
                   "Ssid", SsidValue(ssid),
                   "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifiHelper.Install(wifiPhy, wifiMac, staNodes);

    // Configure one HT station
    WifiHelper wifiHelperHt;
    wifiHelperHt.SetStandard(WIFI_PHY_STANDARD_80211n);
    NetDeviceContainer htStaDevice = wifiHelperHt.Install(wifiPhy, wifiMac, staNodes.Get(0));

    //Configure one 802.11b station
    WifiHelper wifiHelperB;
    wifiHelperB.SetStandard(WIFI_PHY_STANDARD_80211b);
    NetDeviceContainer bStaDevice = wifiHelperB.Install(wifiPhy, wifiMac, staNodes.Get(1));

    if (enableErpProtection) {
        Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/ErpProtection", BooleanValue (true));
    }

    if (useShortPreamble) {
         Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ShortPreambleSupported", BooleanValue (true));
    }

    if (useShortSlotTime) {
         Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/Slot", TimeValue (MicroSeconds (9)));
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
    mobility.Install(apNode);
    mobility.Install(staNodes);

    InternetStackHelper internet;
    internet.Install(apNode);
    internet.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 50000;

    ApplicationContainer clientApps;
    ApplicationContainer serverApps;

    if (protocol == "Udp") {
        UdpServerHelper server(port);
        serverApps = server.Install(apNode.Get(0));
        serverApps.Start(Seconds(0.0));
        serverApps.Stop(Seconds(simulationTime + 1));

        UdpClientHelper client(apInterface.GetAddress(0), port);
        client.SetAttribute("MaxPackets", UintegerValue(4294967295));
        client.SetAttribute("Interval", TimeValue(Time("0.0001"))); //packets/sec
        client.SetAttribute("PacketSize", UintegerValue(payloadSize));

        clientApps.Add(client.Install(staNodes.Get(0)));
        clientApps.Add(client.Install(staNodes.Get(1)));
        clientApps.Add(client.Install(staNodes.Get(2)));
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(simulationTime));

    } else if (protocol == "Tcp") {
         //TCP traffic
        uint16_t sinkPort = port;
        Address sinkAddress (InetSocketAddress (apInterface.GetAddress (0), sinkPort));

        TcpServerHelper tcpServer (sinkPort);
        serverApps = tcpServer.Install (apNode.Get (0));
        serverApps.Start (Seconds (0.0));
        serverApps.Stop (Seconds (simulationTime + 1));

        TcpClientHelper tcpClient (sinkAddress);
        tcpClient.SetAttribute ("MaxPackets", UintegerValue (4294967295));
        tcpClient.SetAttribute ("Interval", TimeValue (Time ("0.0001")));
        tcpClient.SetAttribute ("PacketSize", UintegerValue (payloadSize));

        clientApps.Add (tcpClient.Install (staNodes.Get (0)));
        clientApps.Add (tcpClient.Install (staNodes.Get (1)));
        clientApps.Add (tcpClient.Install (staNodes.Get (2)));
        clientApps.Start (Seconds (1.0));
        clientApps.Stop (Seconds (simulationTime));

    } else {
        std::cerr << "Invalid protocol: " << protocol << std::endl;
        return 1;
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 1));

    if (tracing) {
        wifiPhy.EnablePcapAll("wifi-performance");
    }

    Simulator::Run();

    monitor->CheckForLostPackets ();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

    double totalThroughput = 0.0;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
        totalThroughput += i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
    }

    std::cout << "Total Throughput: " << totalThroughput << " Mbps\n";
    monitor->SerializeToXmlFile("wifi-performance.flowmon", true, true);

    Simulator::Destroy();

    return 0;
}