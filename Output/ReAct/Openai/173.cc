#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridNetworkTopology");

double totalRx = 0;
Time flowStartTime = Seconds(1.0);
Time flowStopTime = Seconds(10.0);

void RxCallback(Ptr<const Packet> packet, const Address &from)
{
    totalRx += packet->GetSize() * 8.0;
}

int main(int argc, char *argv[])
{
    // Parameters
    uint32_t nWired = 4;
    uint32_t nWireless = 3;
    double simTime = 11.0; // seconds

    // Create nodes
    NodeContainer wiredNodes;
    wiredNodes.Create(nWired);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nWireless);

    NodeContainer wifiApNode = NodeContainer(wiredNodes.Get(1)); // Second wired node as AP/gateway

    // Point-to-point chain among wired nodes
    std::vector<NodeContainer> p2pNodeContainers;
    for (uint32_t i = 0; i < nWired - 1; ++i) {
        NodeContainer pair = NodeContainer(wiredNodes.Get(i), wiredNodes.Get(i + 1));
        p2pNodeContainers.push_back(pair);
    }

    // Install point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("50Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    std::vector<NetDeviceContainer> p2pDevices;
    for (auto &pair : p2pNodeContainers) {
        p2pDevices.push_back(p2p.Install(pair));
    }

    // WiFi Network (AP is wiredNodes.Get(1))
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns3-hybrid-wifi");

    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, wifiStaNodes);

    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, wifiApNode);

    // Mobility for wireless nodes
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(5.0),
                                 "DeltaY", DoubleValue(5.0),
                                 "GridWidth", UintegerValue(nWireless),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);

    mobility.Install(wifiApNode);

    // Install Internet
    InternetStackHelper stack;
    stack.Install(wiredNodes);
    stack.Install(wifiStaNodes);

    // Assign IP addresses

    // Wired point-to-point links
    std::vector<Ipv4InterfaceContainer> p2pIfs;
    for (uint32_t i = 0; i < p2pDevices.size(); ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i+1 << ".0";
        Ipv4AddressHelper ipv4;
        ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
        p2pIfs.push_back(ipv4.Assign(p2pDevices[i]));
    }

    // WiFi subnet -- node 1 as AP, assign IPs to wireless nodes and AP device
    Ipv4AddressHelper ipv4wifi;
    ipv4wifi.SetBase("10.2.0.0", "255.255.255.0");
    Ipv4InterfaceContainer staIfs = ipv4wifi.Assign(staDevices);
    Ipv4InterfaceContainer apIf = ipv4wifi.Assign(apDevice);

    // Setup routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install TCP applications
    uint16_t sinkPort = 50000;
    ApplicationContainer sinkApps;
    ApplicationContainer clientApps;

    // Install PacketSink on all wired nodes, each on a unique port
    for (uint32_t i = 0; i < nWired; ++i)
    {
        Address sinkAddress(InetSocketAddress(wiredNodes.Get(i)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), sinkPort + i));
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
        ApplicationContainer sink = packetSinkHelper.Install(wiredNodes.Get(i));
        sink.Start(flowStartTime);
        sink.Stop(flowStopTime);

        sinkApps.Add(sink);
    }

    // Each wireless node sends a TCP flow to a wired node (round-robin mapping)
    for (uint32_t i = 0; i < nWireless; ++i)
    {
        uint32_t dstIdx = i % nWired;
        Ipv4Address dstAddr = wiredNodes.Get(dstIdx)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal();
        AddressValue remoteAddress(InetSocketAddress(dstAddr, sinkPort + dstIdx));
        BulkSendHelper sourceHelper("ns3::TcpSocketFactory", Address());
        sourceHelper.SetAttribute("Remote", remoteAddress);
        sourceHelper.SetAttribute("MaxBytes", UintegerValue(0));
        ApplicationContainer sourceApp = sourceHelper.Install(wifiStaNodes.Get(i));
        sourceApp.Start(flowStartTime + Seconds(0.1 * i));
        sourceApp.Stop(flowStopTime);
        clientApps.Add(sourceApp);
    }

    // Enable pcap tracing on all point-to-point devices and WiFi devices
    for (uint32_t i = 0; i < p2pDevices.size(); ++i)
        p2p.EnablePcapAll("hybrid-p2p" + std::to_string(i), false);

    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    wifiPhy.EnablePcap("hybrid-wifi", staDevices, false);
    wifiPhy.EnablePcap("hybrid-wifi-ap", apDevice, false);

    // Connect packet sinks to track received data
    for (uint32_t i = 0; i < sinkApps.GetN(); ++i)
    {
        Ptr<Application> app = sinkApps.Get(i);
        Ptr<PacketSink> sink = DynamicCast<PacketSink>(app);
        sink->TraceConnectWithoutContext("Rx", MakeCallback(&RxCallback));
    }

    // Enable logging
    //LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    //LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    double throughput = totalRx / (flowStopTime - flowStartTime).GetSeconds() / 1e6; // Mbps

    std::cout << "Average TCP throughput: " << throughput << " Mbps" << std::endl;

    Simulator::Destroy();

    return 0;
}