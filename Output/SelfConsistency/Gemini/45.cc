#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/error-model.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiInterference");

int main(int argc, char *argv[]) {
    bool verbose = false;
    std::string phyMode("DsssRate1Mbps");
    double primaryRss = -80; // dBm
    double interferingRss = -70; // dBm
    double interferenceOffset = 0.000001; // seconds
    uint32_t primaryPacketSize = 1000; // bytes
    uint32_t interferingPacketSize = 500; // bytes
    std::string dataRate = "1Mbps";

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("primaryRss", "Rx Power in dBm for the primary link", primaryRss);
    cmd.AddValue("interferingRss", "Rx Power in dBm for the interfering link", interferingRss);
    cmd.AddValue("interferenceOffset", "Time offset between the primary and interfering signals (seconds)", interferenceOffset);
    cmd.AddValue("primaryPacketSize", "Size of the primary packet (bytes)", primaryPacketSize);
    cmd.AddValue("interferingPacketSize", "Size of the interfering packet (bytes)", interferingPacketSize);

    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
        LogComponentEnable("WifiInterference", LOG_LEVEL_ALL);
    }

    // Configure the wireless channel
    NodeContainer wifiNodes;
    wifiNodes.Create(3);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
    wifiPhy.SetChannel(wifiChannel.Create());

    // Configure the MAC layer
    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("wifi-interference");
    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(1));

    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevices;
    apDevices = wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(0));
    NetDeviceContainer interferenceDevices;
    interferenceDevices = wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(2));


    // Configure mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // Configure the Internet stack
    InternetStackHelper internet;
    internet.Install(wifiNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i = ipv4.Assign(apDevices);
    Ipv4InterfaceContainer j = ipv4.Assign(staDevices);
    Ipv4InterfaceContainer k = ipv4.Assign(interferenceDevices);

    Ipv4Address serverAddress = i.GetAddress(0);

    // Configure the UDP echo application
    UdpEchoClientHelper echoClient(serverAddress, 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(primaryPacketSize));

    ApplicationContainer clientApps = echoClient.Install(wifiNodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(wifiNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Configure the interfering traffic
    UdpEchoClientHelper interferenceClient(serverAddress, 9);
    interferenceClient.SetAttribute("MaxPackets", UintegerValue(1));
    interferenceClient.SetAttribute("Interval", TimeValue(Seconds(1)));
    interferenceClient.SetAttribute("PacketSize", UintegerValue(interferingPacketSize));

    ApplicationContainer interferenceApps = interferenceClient.Install(wifiNodes.Get(2));
    interferenceApps.Start(Seconds(2.0 + interferenceOffset));
    interferenceApps.Stop(Seconds(10.0));


    // Set transmission power (using Friis formula and distance)
    Ptr<FriisPropagationLossModel> friis = CreateObject<FriisPropagationLossModel>();

    //Primary Link
    double distPrimary = 10.0; //Distance between AP and STA
    double txPowerPrimary = primaryRss + friis->GetLoss(distPrimary, 2412e6);
    wifiPhy.SetTxPowerStart(txPowerPrimary);
    wifiPhy.SetTxPowerEnd(txPowerPrimary);

    //Interference Link
    double distInterference = 10.0; //Distance between Interferer and AP
    double txPowerInterference = interferingRss + friis->GetLoss(distInterference, 2412e6);
    wifiPhy.SetTxPowerStart(txPowerInterference);
    wifiPhy.SetTxPowerEnd(txPowerInterference);

    // Disable carrier sense for the interfering node
    Config::Set("/NodeList/2/DeviceList/*/Mac/BE_Txop/CwMin", UintegerValue(0));
    Config::Set("/NodeList/2/DeviceList/*/Mac/BE_Txop/CwMax", UintegerValue(0));

    // Enable PCAP tracing
    wifiPhy.EnablePcap("wifi-interference", apDevices);

    Simulator::Stop(Seconds(11.0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}