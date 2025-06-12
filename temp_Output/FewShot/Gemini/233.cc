#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create nodes
    NodeContainer staNodes;
    staNodes.Create(3);
    NodeContainer apNode;
    apNode.Create(1);

    // Configure Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(1.0)));

    NetDeviceContainer apDevices = wifi.Install(phy, mac, apNode);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // Create UDP client application (Broadcaster)
    UdpClientHelper client(apInterfaces.GetAddress(0), 9);
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("MaxPackets", UintegerValue(10));
    ApplicationContainer clientApps = client.Install(staNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Create UDP server application (Receivers)
    UdpServerHelper server(9);
    ApplicationContainer serverApps = server.Install(apNode);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Enable PCAP tracing
    phy.EnablePcapAll("wifi-broadcast");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}