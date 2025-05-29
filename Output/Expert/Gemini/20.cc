#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/qos-txop.h"
#include "ns3/nqos-wifi-mac-header.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/icmpv4.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool enablePcap = true;

    CommandLine cmd;
    cmd.AddValue("EnablePcap", "Enable or disable packet capture", enablePcap);
    cmd.Parse(argc, argv);

    NodeContainer apNode;
    apNode.Create(1);

    NodeContainer staNodes;
    staNodes.Create(2);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

    NqosWifiMacHelper mac;
    Ssid ssid = Ssid("ns3-80211n-example");

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconGeneration", BooleanValue(true),
                "BeaconInterval", TimeValue(Seconds(0.1)));

    NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

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

    UdpClientHelper client(apInterface.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(staNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    Icmpv4EchoServerHelper server(9);
    ApplicationContainer serverApps = server.Install(apNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));


    QosTxop queue;
    queue.SetBaEnabled(true);
    queue.SetBaThreshold(1);
    queue.SetBaInactivityTimeout(Seconds(5.0));

    Config::Set("/NodeList/*/$ns3::WifiNetDevice/Mac/BE_QosTxop", ObjectValue(queue));

    if (enablePcap) {
        phy.EnablePcap("80211n", apDevice.Get(0));
    }

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();

    Simulator::Destroy();
    return 0;
}