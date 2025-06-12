#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store-module.h"
#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Wifi7Throughput");

double CalculateThroughput (uint64_t bytes, double time)
{
    return (bytes * 8.0) / (time * 1e6);
}

int main (int argc, char *argv[])
{
    bool verbose = false;
    uint32_t nWifi = 3;
    std::string trafficType = "TCP";
    double simulationTime = 10.0;
    uint32_t payloadSize = 1472;
    uint32_t mcs = 11;
    uint32_t channelWidth = 160;
    std::string guardInterval = "0.8";
    std::string frequency = "5";
    bool uplinkOfdma = false;
    bool bsrp = false;
    uint32_t mpduBufferSize = 8192;
    std::string phyMode = "Ht80";
    double expectedMinThroughput = 100.0;
    double expectedMaxThroughput = 1000.0;
    double tolerance = 0.1;

    CommandLine cmd;
    cmd.AddValue ("verbose", "Tell application to log if true.", verbose);
    cmd.AddValue ("nWifi", "Number of wifi STA devices", nWifi);
    cmd.AddValue ("trafficType", "Traffic type (TCP or UDP)", trafficType);
    cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue ("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue ("mcs", "MCS value (0-11 for HE, 0-13 for EHT)", mcs);
    cmd.AddValue ("channelWidth", "Channel width (20, 40, 80, 160)", channelWidth);
    cmd.AddValue ("guardInterval", "Guard interval (0.8, 1.6, 3.2)", guardInterval);
    cmd.AddValue ("frequency", "Operating frequency (2.4, 5, 6)", frequency);
    cmd.AddValue ("uplinkOfdma", "Enable Uplink OFDMA", uplinkOfdma);
    cmd.AddValue ("bsrp", "Enable BSRP", bsrp);
    cmd.AddValue ("mpduBufferSize", "MPDU buffer size", mpduBufferSize);
    cmd.AddValue ("phyMode", "PHY mode (Ht20, Vht80, He160, Eht320)", phyMode);
    cmd.AddValue ("expectedMinThroughput", "Expected minimum throughput (Mbps)", expectedMinThroughput);
    cmd.AddValue ("expectedMaxThroughput", "Expected maximum throughput (Mbps)", expectedMaxThroughput);
    cmd.AddValue ("tolerance", "Tolerance for throughput validation", tolerance);
    cmd.Parse (argc, argv);

    if (verbose)
    {
        LogComponentEnable ("Wifi7Throughput", LOG_LEVEL_INFO);
    }

    NodeContainer staNodes;
    staNodes.Create (nWifi);
    NodeContainer apNode;
    apNode.Create (1);

    WifiHelper wifiHelper;
    wifiHelper.SetStandard (WIFI_PHY_STANDARD_80211be);

    YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default ();
    phyHelper.SetChannel (channelHelper.Create ());

    WifiMacHelper macHelper;
    Ssid ssid = Ssid ("ns3-wifi7");

    // Configure AP
    macHelper.SetType ("ns3::ApWifiMac",
                       "Ssid", SsidValue (ssid),
                       "BeaconGeneration", BooleanValue (true),
                       "BeaconInterval", TimeValue (Seconds (0.1)));

    NetDeviceContainer apDevice = wifiHelper.Install (phyHelper, macHelper, apNode);

    // Configure STAs
    macHelper.SetType ("ns3::StaWifiMac",
                       "Ssid", SsidValue (ssid),
                       "ActiveProbing", BooleanValue (false));

    NetDeviceContainer staDevices = wifiHelper.Install (phyHelper, macHelper, staNodes);

    // Configure PHY
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (channelWidth));
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/GuardInterval", StringValue (guardInterval));

    // Set operating frequency
    uint32_t channelNumber = 36;
    if (frequency == "2.4") {
        channelNumber = 1;
    } else if (frequency == "5") {
        channelNumber = 36;
    } else if (frequency == "6") {
        channelNumber = 1;
    } else {
        std::cerr << "Invalid frequency. Defaulting to 5 GHz." << std::endl;
    }

    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelNumber", UintegerValue (channelNumber));
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/OperatingFrequency", StringValue (frequency));

    if (uplinkOfdma) {
        Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/EnableUplinkOfdma", BooleanValue (true));
    }

    if (bsrp) {
        Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/EnableBsrp", BooleanValue (true));
    }

    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MpduBufferSize", UintegerValue (mpduBufferSize));

    // Set MCS
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/HeMcs", UintegerValue (mcs));
    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue (0.0),
                                     "MinY", DoubleValue (0.0),
                                     "DeltaX", DoubleValue (5.0),
                                     "DeltaY", DoubleValue (10.0),
                                     "GridWidth", UintegerValue (3),
                                     "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (staNodes);

    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue (10.0),
                                     "MinY", DoubleValue (10.0),
                                     "DeltaX", DoubleValue (0.0),
                                     "DeltaY", DoubleValue (0.0),
                                     "GridWidth", UintegerValue (1),
                                     "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (apNode);

    // Install Internet Stack
    InternetStackHelper stack;
    stack.Install (apNode);
    stack.Install (staNodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign (apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    // Create Applications
    uint16_t port = 50000;
    Address serverAddress = InetSocketAddress (apInterface.GetAddress (0), port);

    ApplicationContainer clientApps;
    ApplicationContainer serverApps;

    if (trafficType == "TCP")
    {
        TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
        for (uint32_t i = 0; i < nWifi; ++i)
        {
            BulkSendHelper source (tid, serverAddress);
            source.SetAttribute ("MaxBytes", UintegerValue (0));
            source.SetAttribute ("SendSize", UintegerValue (payloadSize));
            clientApps.Add (source.Install (staNodes.Get (i)));

            PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
            serverApps.Add (sink.Install (apNode.Get (0)));
        }
    }
    else if (trafficType == "UDP")
    {
        TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
        for (uint32_t i = 0; i < nWifi; ++i)
        {
            UdpClientHelper client (apInterface.GetAddress (0), port);
            client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
            client.SetAttribute ("Interval", TimeValue (Time (Seconds (0.00002))));
            client.SetAttribute ("PacketSize", UintegerValue (payloadSize));
            clientApps.Add (client.Install (staNodes.Get (i)));

            PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
            serverApps.Add (sink.Install (apNode.Get (0)));
        }
    }
    else
    {
        std::cerr << "Invalid traffic type. Must be TCP or UDP." << std::endl;
        return 1;
    }

    clientApps.Start (Seconds (1.0));
    serverApps.Start (Seconds (0.0));

    // Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

    Simulator::Stop (Seconds (simulationTime + 1));

    Simulator::Run ();

    monitor->CheckForLostPackets ();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

    double totalThroughput = 0.0;
    bool errorOccurred = false;

    std::cout << "-------------------------------------------------------------------------------------------------------" << std::endl;
    std::cout << "MCS | Channel Width | Guard Interval | Throughput (Mbps)" << std::endl;
    std::cout << "-------------------------------------------------------------------------------------------------------" << std::endl;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
        double throughput = CalculateThroughput (i->second.rxBytes, simulationTime);
        totalThroughput += throughput;

        if (throughput < expectedMinThroughput * (1 - tolerance) || throughput > expectedMaxThroughput * (1 + tolerance))
        {
            std::cerr << "Error: Throughput is outside the expected range for MCS " << mcs
                      << ", Channel Width " << channelWidth << ", Guard Interval " << guardInterval << std::endl;
            errorOccurred = true;
        }

        std::cout << std::setw(3) << mcs << " | " << std::setw(13) << channelWidth << " | " << std::setw(14) << guardInterval
                  << " | " << std::setw(16) << std::fixed << std::setprecision(2) << throughput << std::endl;
    }
    std::cout << "-------------------------------------------------------------------------------------------------------" << std::endl;

    monitor->SerializeToXmlFile ("wifi7-throughput.flowmon", true, true);

    Simulator::Destroy ();

    if (errorOccurred)
    {
        return 1;
    }

    return 0;
}