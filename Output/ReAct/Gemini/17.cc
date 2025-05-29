#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include <sstream>

using namespace ns3;

int main(int argc, char *argv[]) {

    bool verbose = false;
    bool enableRtsCts = false;
    uint32_t payloadSize = 1472;
    std::string dataRate = "54Mbps";
    std::string phyRate = "OfdmRate54Mbps";
    double simulationTime = 10;
    double distance = 10;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if all packets are sent", verbose);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("dataRate", "Data rate for TCP agents", dataRate);
    cmd.AddValue("phyRate", "Physical layer rate", phyRate);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("distance", "Distance between AP and STA", distance);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiMacQueue::MaxSize", StringValue("50000000000000"));
    Config::SetDefault("ns3::WifiMacQueue::Mode", EnumValue(WifiMacQueue::ADAPTIVE));

    NodeContainer apNodes;
    apNodes.Create(4);
    NodeContainer staNodes;
    staNodes.Create(4);

    YansWifiChannelHelper channel[4];
    YansWifiPhyHelper phy[4];
    WifiHelper wifi[4];
    WifiMacHelper mac[4];

    std::stringstream ss;
    ss << "channel width=20MHz";

    for (int i = 0; i < 4; ++i) {
        channel[i] = YansWifiChannelHelper::Default();
        channel[i].AddPropagationLoss("ns3::FriisPropagationLossModel");

        phy[i] = YansWifiPhyHelper::Default();
        phy[i].SetChannel(channel[i].Create());

        wifi[i] = WifiHelper::Default();

        mac[i] = WifiMacHelper::Default();
    }

    Ssid ssid[4];
    NetDeviceContainer staDevices[4];
    NetDeviceContainer apDevices[4];

    for (int i = 0; i < 4; ++i) {
        ssid[i] = Ssid("ns3-wifi");
        mac[i].SetType("ns3::StaWifiMac",
                       "Ssid", SsidValue(ssid[i]),
                       "ActiveProbing", BooleanValue(false));

        staDevices[i] = wifi[i].Install(phy[i], mac[i], staNodes.Get(i));

        mac[i].SetType("ns3::ApWifiMac",
                       "Ssid", SsidValue(ssid[i]));

        apDevices[i] = wifi[i].Install(phy[i], mac[i], apNodes.Get(i));
    }
    
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelNumber", UintegerValue (36));
    Config::Set ("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelNumber", UintegerValue (40));
    Config::Set ("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelNumber", UintegerValue (44));
    Config::Set ("/NodeList/3/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelNumber", UintegerValue (48));
    
    if (enableRtsCts) {
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_Edca/RtsCtsThreshold", UintegerValue (0));
    }
    
    // Set aggregation parameters for station A
    Config::Set ("/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_Edca/AmsduEnable", BooleanValue (false));
    Config::Set ("/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_Edca/AmpduEnable", BooleanValue (true));
    Config::Set ("/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/MaxAmpduLength", UintegerValue (65535));

    // Set aggregation parameters for station B
    Config::Set ("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_Edca/AmsduEnable", BooleanValue (false));
    Config::Set ("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_Edca/AmpduEnable", BooleanValue (false));

    // Set aggregation parameters for station C
    Config::Set ("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_Edca/AmsduEnable", BooleanValue (true));
    Config::Set ("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_Edca/AmpduEnable", BooleanValue (false));
    Config::Set ("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_Edca/AmsduMaxSize", UintegerValue (8192));

    // Set aggregation parameters for station D
    Config::Set ("/NodeList/3/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_Edca/AmsduEnable", BooleanValue (true));
    Config::Set ("/NodeList/3/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_Edca/AmpduEnable", BooleanValue (true));
    Config::Set ("/NodeList/3/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_Edca/AmpduMaxSize", UintegerValue (4096));
    Config::Set ("/NodeList/3/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/MaxAmpduLength", UintegerValue (32768));

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(distance),
                                  "GridWidth", UintegerValue(4),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(distance * 1.5),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(distance),
                                  "GridWidth", UintegerValue(4),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes);

    InternetStackHelper internet;
    internet.Install(apNodes);
    internet.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staNodeIface[4];
    Ipv4InterfaceContainer apNodeIface[4];

    for (int i = 0; i < 4; ++i) {
        staNodeIface[i] = address.Assign(staDevices[i]);
        apNodeIface[i] = address.Assign(apDevices[i]);
        address.NewNetwork();
        address.SetBase("10.1." + std::to_string(i+2) + ".0", "255.255.255.0");
    }
    
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    
    uint16_t port = 9;

    OnOffHelper onoff("ns3::UdpSocketFactory",
                      Address(InetSocketAddress(apNodeIface[0].GetAddress(0), port)));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
    onoff.SetAttribute("DataRate", StringValue(dataRate));

    ApplicationContainer apps = onoff.Install(staNodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(simulationTime + 1));

    for (int i = 1; i < 4; ++i) {
        OnOffHelper onoff_i("ns3::UdpSocketFactory",
                      Address(InetSocketAddress(apNodeIface[i].GetAddress(0), port)));
        onoff_i.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff_i.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff_i.SetAttribute("PacketSize", UintegerValue(payloadSize));
        onoff_i.SetAttribute("DataRate", StringValue(dataRate));
        ApplicationContainer apps_i = onoff_i.Install(staNodes.Get(i));
        apps_i.Start(Seconds(1.0));
        apps_i.Stop(Seconds(simulationTime + 1));
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 2));

    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    if (verbose) {
        for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
            std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
            std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
            std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
            std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000 << " Mbps\n";
        }
    }

    Simulator::Destroy();

    return 0;
}