#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/packet-sink-helper.h"

using namespace ns3;

void PrintStationStats(std::vector<ApplicationContainer> &clientApps, std::vector<uint64_t> &lastBytes, double interval)
{
    for (size_t i = 0; i < clientApps.size(); ++i)
    {
        Ptr<UdpClient> client = DynamicCast<UdpClient>(clientApps[i].Get(0));
        if (client)
        {
            uint64_t total = client->GetSent();
            std::cout << "Time: " << Simulator::Now().GetSeconds()
                      << "s, STA " << i << ": Total Packets Sent = " << total
                      << ", New Packets = " << total - lastBytes[i] << std::endl;
            lastBytes[i] = total;
        }
    }
    Simulator::Schedule(Seconds(interval), &PrintStationStats, std::ref(clientApps), std::ref(lastBytes), interval);
}

int main(int argc, char *argv[])
{
    uint32_t numStaPerAp = 3;
    double simulationTime = 10.0;
    double interPacketInterval = 0.05;
    uint32_t packetSize = 1024;

    CommandLine cmd;
    cmd.AddValue("numStaPerAp", "Number of stations per AP", numStaPerAp);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    uint32_t numAps = 2;
    uint32_t numStas = numAps * numStaPerAp;

    NodeContainer wifiApNodes;
    wifiApNodes.Create(numAps);
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(numStas);

    // Configure Wi-Fi channel and physical layer
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("DsssRate11Mbps"));

    WifiMacHelper mac;
    Ssid ssid1 = Ssid("ns3-ap-1");
    Ssid ssid2 = Ssid("ns3-ap-2");

    NetDeviceContainer staDevices1, apDevice1, staDevices2, apDevice2;

    // AP1 group
    NodeContainer staGroup1;
    for (uint32_t i = 0; i < numStaPerAp; ++i)
        staGroup1.Add(wifiStaNodes.Get(i));

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1));
    staDevices1 = wifi.Install(phy, mac, staGroup1);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    apDevice1 = wifi.Install(phy, mac, wifiApNodes.Get(0));

    // AP2 group
    NodeContainer staGroup2;
    for (uint32_t i = 0; i < numStaPerAp; ++i)
        staGroup2.Add(wifiStaNodes.Get(i + numStaPerAp));

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2));
    staDevices2 = wifi.Install(phy, mac, staGroup2);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    apDevice2 = wifi.Install(phy, mac, wifiApNodes.Get(1));

    // Mobility setup
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
    posAlloc->Add(Vector(0.0, 0.0, 0.0));
    posAlloc->Add(Vector(50.0, 0.0, 0.0));
    mobility.SetPositionAllocator(posAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNodes);

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(20.0),
                                 "DeltaX", DoubleValue(5.0),
                                 "DeltaY", DoubleValue(20.0),
                                 "GridWidth", UintegerValue(numStaPerAp),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(-100, 100, 0, 100)));
    mobility.Install(wifiStaNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNodes);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    // AP1/STA1 group
    address.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staIf1, apIf1, staIf2, apIf2;
    staIf1 = address.Assign(staDevices1);
    apIf1 = address.Assign(apDevice1);

    // AP2/STA2 group
    address.SetBase("10.0.2.0", "255.255.255.0");
    staIf2 = address.Assign(staDevices2);
    apIf2 = address.Assign(apDevice2);

    // UDP servers - one server behind each AP, listening on port 9000
    uint16_t port = 9000;
    UdpServerHelper server1(port);
    ApplicationContainer serverApp1 = server1.Install(wifiApNodes.Get(0));
    serverApp1.Start(Seconds(0.0));
    serverApp1.Stop(Seconds(simulationTime + 1));

    UdpServerHelper server2(port);
    ApplicationContainer serverApp2 = server2.Install(wifiApNodes.Get(1));
    serverApp2.Start(Seconds(0.0));
    serverApp2.Stop(Seconds(simulationTime + 1));

    // UDP clients - each STA sends to its AP's server
    std::vector<ApplicationContainer> clientApps(numStas);
    uint32_t staIdx = 0;

    for (uint32_t i = 0; i < numStaPerAp; ++i)
    {
        UdpClientHelper client(staIf1.GetAddress(0), port); // AP1 server
        client.SetAttribute("MaxPackets", UintegerValue(0)); // continuous
        client.SetAttribute("Interval", TimeValue(Seconds(interPacketInterval)));
        client.SetAttribute("PacketSize", UintegerValue(packetSize));

        clientApps[staIdx] = client.Install(staGroup1.Get(i));
        clientApps[staIdx].Start(Seconds(1.0));
        clientApps[staIdx].Stop(Seconds(simulationTime));
        staIdx++;
    }

    for (uint32_t i = 0; i < numStaPerAp; ++i)
    {
        UdpClientHelper client(staIf2.GetAddress(0), port); // AP2 server
        client.SetAttribute("MaxPackets", UintegerValue(0)); // continuous
        client.SetAttribute("Interval", TimeValue(Seconds(interPacketInterval)));
        client.SetAttribute("PacketSize", UintegerValue(packetSize));

        clientApps[staIdx] = client.Install(staGroup2.Get(i));
        clientApps[staIdx].Start(Seconds(1.0));
        clientApps[staIdx].Stop(Seconds(simulationTime));
        staIdx++;
    }

    // Enable tracing
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.EnablePcap("wifi-ap1", apDevice1);
    phy.EnablePcap("wifi-ap2", apDevice2);
    phy.EnablePcap("wifi-sta1", staDevices1);
    phy.EnablePcap("wifi-sta2", staDevices2);

    AsciiTraceHelper ascii;
    phy.EnableAscii(ascii.CreateFileStream("wifi-ap1.tr"), apDevice1);
    phy.EnableAscii(ascii.CreateFileStream("wifi-ap2.tr"), apDevice2);
    phy.EnableAscii(ascii.CreateFileStream("wifi-sta1.tr"), staDevices1);
    phy.EnableAscii(ascii.CreateFileStream("wifi-sta2.tr"), staDevices2);

    // Station stats
    std::vector<uint64_t> lastBytes(numStas, 0);
    Simulator::Schedule(Seconds(1.0), &PrintStationStats, std::ref(clientApps), std::ref(lastBytes), 1.0);

    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();

    // Final print of data sent per station
    std::cout << std::endl << "====== Total Packets Sent Per Station ======" << std::endl;
    for (size_t i = 0; i < clientApps.size(); ++i)
    {
        Ptr<UdpClient> client = DynamicCast<UdpClient>(clientApps[i].Get(0));
        if (client)
        {
            std::cout << "STA " << i << " total packets sent: " << client->GetSent() << std::endl;
        }
    }

    Simulator::Destroy();
    return 0;
}