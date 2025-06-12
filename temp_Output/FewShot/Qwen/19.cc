#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

WifiStandard ConvertStringToWifiStandard(std::string version, WifiPhyBand *band) {
    if (version == "80211a") {
        *band = WIFI_PHY_BAND_5GHZ;
        return WIFI_STANDARD_80211a;
    } else if (version == "80211b") {
        *band = WIFI_PHY_BAND_2_4GHZ;
        return WIFI_STANDARD_80211b;
    } else if (version == "80211g") {
        *band = WIFI_PHY_BAND_2_4GHZ;
        return WIFI_STANDARD_80211g;
    } else if (version == "80211n") {
        *band = WIFI_PHY_BAND_2_4GHZ; // or 5GHz depending on context
        return WIFI_STANDARD_80211n;
    } else if (version == "80211ac") {
        *band = WIFI_PHY_BAND_5GHZ;
        return WIFI_STANDARD_80211ac;
    } else if (version == "80211ax") {
        *band = WIFI_PHY_BAND_2_4GHZ; // can be either
        return WIFI_STANDARD_80211ax;
    } else {
        NS_FATAL_ERROR("Unknown Wi-Fi standard: " << version);
    }
}

int main(int argc, char *argv[]) {
    std::string apVersion = "80211ac";
    std::string staVersion = "80211n";
    std::string trafficDirection = "downlink"; // uplink, downlink, or bidirectional
    double simTime = 10.0;

    CommandLine cmd;
    cmd.AddValue("apVersion", "Wi-Fi standard for AP (e.g., 80211a, 80211ac)", apVersion);
    cmd.AddValue("staVersion", "Wi-Fi standard for STA (e.g., 80211n)", staVersion);
    cmd.AddValue("trafficDirection", "Traffic direction: uplink, downlink, or bidirectional", trafficDirection);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.Parse(argc, argv);

    NodeContainer wifiApNode;
    wifiApNode.Create(1);
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(1);

    WifiPhyBand apBand, staBand;
    WifiStandard apStandard = ConvertStringToWifiStandard(apVersion, &apBand);
    WifiStandard staStandard = ConvertStringToWifiStandard(staVersion, &staBand);

    YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();
    phyHelper.SetChannel(channelHelper.Create());

    WifiMacHelper macHelper;
    WifiHelper wifiHelper;

    // Configure AP
    wifiHelper.SetStandard(apStandard);
    wifiHelper.SetRemoteStationManager("ns3::MinstrelWifiManager");
    macHelper.SetType("ns3::ApWifiMac");

    NetDeviceContainer apDevice = wifiHelper.Install(phyHelper, macHelper, wifiApNode);

    // Configure STA
    wifiHelper.SetStandard(staStandard);
    wifiHelper.SetRemoteStationManager("ns3::AarfWifiManager");
    macHelper.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("wifi-network")));

    NetDeviceContainer staDevice = wifiHelper.Install(phyHelper, macHelper, wifiStaNodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

    // Global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Traffic generation
    uint16_t port = 9;

    if (trafficDirection == "downlink" || trafficDirection == "bidirectional") {
        // Downlink: from AP to STA
        UdpEchoServerHelper echoServer(port);
        ApplicationContainer serverApp = echoServer.Install(wifiStaNodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(simTime));

        UdpEchoClientHelper echoClient(staInterface.GetAddress(0), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = echoClient.Install(wifiApNode.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(simTime));
    }

    if (trafficDirection == "uplink" || trafficDirection == "bidirectional") {
        // Uplink: from STA to AP
        UdpEchoServerHelper echoServer(port + 1);
        ApplicationContainer serverApp = echoServer.Install(wifiApNode.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(simTime));

        UdpEchoClientHelper echoClient(apInterface.GetAddress(0), port + 1);
        echoClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = echoClient.Install(wifiStaNodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(simTime));
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}