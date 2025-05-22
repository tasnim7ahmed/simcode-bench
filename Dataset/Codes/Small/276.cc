#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Set the number of stations
    uint32_t numSta = 3;

    // Create nodes: 1 AP and several STAs
    NodeContainer apNode, staNodes;
    apNode.Create(1);
    staNodes.Create(numSta);

    // Set up the Wi-Fi devices
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("wifi-network");
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(2)));

    WifiDeviceHelper wifiDevices;
    wifiDevices.SetDeviceAttribute("WifiPhy", PointerValue(&phy));
    NetDeviceContainer apDevice = wifiDevices.Install(apNode);
    NetDeviceContainer staDevices = wifiDevices.Install(staNodes);

    // Set mobility model (static AP and random STAs)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(apNode);
    internet.Install(staNodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = ipv4.Assign(staDevices);
    Ipv4InterfaceContainer apInterfaces = ipv4.Assign(apDevice);

    // Create a Simplex UDP server on AP
    UdpServerHelper udpServer(9);
    ApplicationContainer serverApp = udpServer.Install(apNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Create Simplex UDP clients on STAs
    UdpClientHelper udpClient(apInterfaces.GetAddress(0), 9);
    udpClient.SetAttribute("MaxPackets", UintegerValue(5));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numSta; i++)
    {
        clientApps.Add(udpClient.Install(staNodes.Get(i)));
    }

    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
