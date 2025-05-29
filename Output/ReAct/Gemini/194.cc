#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiExample");

int main(int argc, char *argv[]) {

    bool enablePcap = true;
    bool enableAsciiTrace = true;
    uint32_t numSta1 = 3;
    uint32_t numSta2 = 3;
    double simulationTime = 10.0;
    std::string dataRate = "50Mbps";
    uint16_t port = 9;

    CommandLine cmd;
    cmd.AddValue("numSta1", "Number of STA devices in group 1", numSta1);
    cmd.AddValue("numSta2", "Number of STA devices in group 2", numSta2);
    cmd.AddValue("enablePcap", "Enable pcap tracing", enablePcap);
    cmd.AddValue("enableAsciiTrace", "Enable ASCII tracing", enableAsciiTrace);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    NodeContainer apNodes;
    apNodes.Create(2);

    NodeContainer staNodes1;
    staNodes1.Create(numSta1);

    NodeContainer staNodes2;
    staNodes2.Create(numSta2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

    WifiMacHelper mac;
    Ssid ssid1 = Ssid("ns-3-ssid1");
    Ssid ssid2 = Ssid("ns-3-ssid2");

    // Configure AP1
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid1),
                "BeaconInterval", TimeValue(Seconds(0.075)));
    YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy1 = YansWifiPhyHelper::Default();
    phy1.SetChannel(channel1.Create());
    WifiHelper wifi1 = wifi;
    wifi1.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HtMcs7"),
                                 "ControlMode", StringValue("HtMcs0"));

    NetDeviceContainer apDevice1 = wifi1.Install(phy1, mac, apNodes.Get(0));

    // Configure STA1
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid1),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice1 = wifi1.Install(phy1, mac, staNodes1);

    // Configure AP2
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid2),
                "BeaconInterval", TimeValue(Seconds(0.075)));

    YansWifiChannelHelper channel2 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy2 = YansWifiPhyHelper::Default();
    phy2.SetChannel(channel2.Create());
    WifiHelper wifi2 = wifi;
    wifi2.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HtMcs7"),
                                 "ControlMode", StringValue("HtMcs0"));

    NetDeviceContainer apDevice2 = wifi2.Install(phy2, mac, apNodes.Get(1));

    // Configure STA2
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid2),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice2 = wifi2.Install(phy2, mac, staNodes2);

    // Mobility
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
    mobility.Install(staNodes1);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes2);

    // Internet stack
    InternetStackHelper internet;
    internet.Install(apNodes);
    internet.Install(staNodes1);
    internet.Install(staNodes2);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces1 = address.Assign(apDevice1);
    Ipv4InterfaceContainer staInterfaces1 = address.Assign(staDevice1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces2 = address.Assign(apDevice2);
    Ipv4InterfaceContainer staInterfaces2 = address.Assign(staDevice2);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP application
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(apNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simulationTime));

    UdpClientHelper echoClient1(apInterfaces1.GetAddress(0), port);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(4294967295));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(0.00002))); // Adjust interval for desired data rate
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps1;
    for (uint32_t i = 0; i < numSta1; ++i) {
        clientApps1.Add(echoClient1.Install(staNodes1.Get(i)));
    }
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(simulationTime));

    UdpClientHelper echoClient2(apInterfaces2.GetAddress(0), port);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(4294967295));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(0.00002))); // Adjust interval for desired data rate
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps2;
    for (uint32_t i = 0; i < numSta2; ++i) {
        clientApps2.Add(echoClient2.Install(staNodes2.Get(i)));
    }
    clientApps2.Start(Seconds(2.0));
    clientApps2.Stop(Seconds(simulationTime));

    // Tracing
    if (enablePcap) {
        phy1.EnablePcap("wifi-ap1", apDevice1.Get(0));
        phy2.EnablePcap("wifi-ap2", apDevice2.Get(0));
        for (uint32_t i = 0; i < numSta1; ++i) {
            phy1.EnablePcap("wifi-sta1-" + std::to_string(i), staDevice1.Get(i));
        }
        for (uint32_t i = 0; i < numSta2; ++i) {
            phy2.EnablePcap("wifi-sta2-" + std::to_string(i), staDevice2.Get(i));
        }
    }

    if (enableAsciiTrace) {
        AsciiTraceHelper ascii;
        phy1.EnableAsciiAll(ascii.CreateFileStream("wifi-trace.txt"));
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Print stats
    for (uint32_t i = 0; i < numSta1; ++i) {
      Ptr<Application> app = clientApps1.Get(i);
      Ptr<UdpClient> client = DynamicCast<UdpClient>(app);
      std::cout << "STA1-" << i << " sent " << client->GetTotalTxBytes() << " bytes" << std::endl;
    }

    for (uint32_t i = 0; i < numSta2; ++i) {
      Ptr<Application> app = clientApps2.Get(i);
      Ptr<UdpClient> client = DynamicCast<UdpClient>(app);
      std::cout << "STA2-" << i << " sent " << client->GetTotalTxBytes() << " bytes" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}