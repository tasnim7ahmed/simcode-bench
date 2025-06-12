#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridWiredWireless");

int main(int argc, char *argv[])
{
    // Simulation parameters
    double simTime = 15.0;

    // Create wired and wireless nodes
    NodeContainer wiredNodes;
    wiredNodes.Create(4);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(3);

    Ptr<Node> wifiApNode = wiredNodes.Get(0); // Gateway node

    // Point-to-point chain connections
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("50Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevices01 = p2p.Install(wiredNodes.Get(0), wiredNodes.Get(1));
    NetDeviceContainer p2pDevices12 = p2p.Install(wiredNodes.Get(1), wiredNodes.Get(2));
    NetDeviceContainer p2pDevices23 = p2p.Install(wiredNodes.Get(2), wiredNodes.Get(3));

    // Wi-Fi (infra, AP is wiredNodes.Get(0))
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("hybrid-ssid");

    // Stations
    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevices =
        wifi.Install(wifiPhy, wifiMac, wifiStaNodes);

    // AP
    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice =
        wifi.Install(wifiPhy, wifiMac, wifiApNode);

    // Mobility for wireless nodes
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(2.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(12.0),
                                 "DeltaY", DoubleValue(10.0),
                                 "GridWidth", UintegerValue(3),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(0.0),
                                 "DeltaY", DoubleValue(0.0),
                                 "GridWidth", UintegerValue(1),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.Install(wifiApNode);

    // Stack and IP assignment
    InternetStackHelper stack;
    stack.Install(wiredNodes);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;

    // Assign IPs on p2p links
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if_p2p01 = address.Assign(p2pDevices01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if_p2p12 = address.Assign(p2pDevices12);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer if_p2p23 = address.Assign(p2pDevices23);

    // Assign IPs for Wi-Fi
    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer staIfs = address.Assign(staDevices);
    Ipv4InterfaceContainer apIf = address.Assign(apDevice);

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // TCP application: each wireless node --> last wired node
    uint16_t port = 50000;
    ApplicationContainer serverApps, clientApps;

    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
    {
        // Install packet sink on last wired node
        Address sinkLocalAddress(InetSocketAddress(if_p2p23.GetAddress(1), port + i));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
        ApplicationContainer serverApp = sinkHelper.Install(wiredNodes.Get(3));
        serverApp.Start(Seconds(0.0));
        serverApp.Stop(Seconds(simTime));
        serverApps.Add(serverApp);

        // Install TCP OnOffApplication on STA
        OnOffHelper clientHelper("ns3::TcpSocketFactory",
                                 InetSocketAddress(if_p2p23.GetAddress(1), port + i));
        clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("20Mbps")));
        clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
        clientHelper.SetAttribute("StartTime", TimeValue(Seconds(1.0 + 0.5*i)));
        clientHelper.SetAttribute("StopTime", TimeValue(Seconds(simTime)));
        clientApps.Add(clientHelper.Install(wifiStaNodes.Get(i)));
    }

    // Enable PCAP tracing
    p2p.EnablePcapAll("p2p-hybrid");
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    wifiPhy.EnablePcap("wifi-hybrid", apDevice.Get(0));
    for (uint32_t i = 0; i < staDevices.GetN(); ++i)
    {
        wifiPhy.EnablePcap("wifi-hybrid", staDevices.Get(i));
    }

    // Throughput calculation
    std::vector<Ptr<PacketSink>> sinks;
    for (uint32_t i = 0; i < serverApps.GetN(); ++i)
    {
        sinks.push_back(DynamicCast<PacketSink>(serverApps.Get(i).Get(0)));
    }
    uint64_t lastTotalRx = 0;

    Simulator::Schedule(Seconds(simTime), [&]()
    {
        uint64_t totalRx = 0;
        for (auto s : sinks) totalRx += s->GetTotalRx();
        double throughput = (totalRx * 8.0) / (simTime - 1.0) / 1e6; // exclude app start delay
        std::cout << "Average TCP throughput: " << throughput << " Mbps" << std::endl;
    });

    Simulator::Stop(Seconds(simTime + 0.2));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}