#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridNetworkSimulation");

int main(int argc, char *argv[]) {
    uint32_t simulationTime = 10;
    std::string phyMode("DsssRate1Mbps");

    NodeContainer wiredNodes;
    wiredNodes.Create(4);

    NodeContainer wirelessNodes;
    wirelessNodes.Create(3);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer wiredDevices;
    for (uint32_t i = 0; i < 3; ++i) {
        NetDeviceContainer container = p2p.Install(NodeContainer(wiredNodes.Get(i), wiredNodes.Get(i + 1)));
        wiredDevices.Add(container);
    }

    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(channel.Create());

    wifiMac.SetType("ns3::AdhocWifiMac");
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));

    NetDeviceContainer wirelessDevices = wifi.Install(wifiPhy, wifiMac, wirelessNodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wiredNodes);
    mobility.Install(wirelessNodes);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer wiredInterfaces;
    for (uint32_t i = 0; i < wiredDevices.GetN(); ++i) {
        Ipv4InterfaceContainer ifc = address.Assign(wiredDevices.Get(i));
        wiredInterfaces.Add(ifc);
        address.NewNetwork();
    }

    Ipv4InterfaceContainer wirelessInterfaces = address.Assign(wirelessDevices);

    Ipv4StaticRoutingHelper ipv4StaticRouting;

    Ptr<Ipv4> ipv4WirelessNode = wirelessNodes.Get(0)->GetObject<Ipv4>();
    uint32_t gatewayInterface = ipv4WirelessNode->AddInterface(wiredNodes.Get(0)->GetObject<Ipv4>()->GetInterfaceForDevice(wiredDevices.Get(0).Get(0)));
    Ipv4InterfaceAddress interfaceAddr = Ipv4InterfaceAddress("192.168.1.1", "255.255.255.0");
    ipv4WirelessNode->AddAddress(gatewayInterface, interfaceAddr);
    ipv4WirelessNode->SetMetric(gatewayInterface, 1);
    ipv4WirelessNode->SetUp(gatewayInterface);

    for (uint32_t i = 0; i < wirelessNodes.GetN(); ++i) {
        Ptr<Ipv4> ipv4Wireless = wirelessNodes.Get(i)->GetObject<Ipv4>();
        for (uint32_t j = 0; j < wiredNodes.GetN(); ++j) {
            Ipv4Address targetNetwork("10.1.1.0");
            Ipv4Mask targetMask("255.255.255.0");
            Ipv4Address nextHop("192.168.1.1");
            ipv4StaticRouting.AddNetworkRouteTo(ipv4Wireless, targetNetwork, targetMask, nextHop, 1);
        }
    }

    uint16_t port = 50000;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);

    ApplicationContainer sinkApps;
    for (uint32_t i = 0; i < wiredNodes.GetN(); ++i) {
        ApplicationContainer app = packetSinkHelper.Install(wiredNodes.Get(i));
        app.Start(Seconds(0.0));
        app.Stop(Seconds(simulationTime));
        sinkApps.Add(app);
    }

    OnOffHelper onoff("ns3::TcpSocketFactory", Address());
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("100Kbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1000));

    ApplicationContainer sourceApps;
    for (uint32_t i = 0; i < wirelessNodes.GetN(); ++i) {
        AddressValue remoteAddress(InetSocketAddress(wiredInterfaces.GetAddress(0, 0), port));
        onoff.SetAttribute("Remote", remoteAddress);
        ApplicationContainer app = onoff.Install(wirelessNodes.Get(i));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(simulationTime - 0.1));
        sourceApps.Add(app);
    }

    p2p.EnablePcapAll("hybrid_network_p2p");
    wifiPhy.EnablePcap("hybrid_network_wifi", wirelessDevices.Get(0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();

    double averageThroughput = 0;
    uint32_t flowCount = 0;

    for (auto flowId : monitor->GetFlowIds()) {
        FlowMonitor::FlowStats stats = monitor->GetFlowStats(flowId);
        if (stats.rxBytes > 0) {
            averageThroughput += (stats.rxBytes * 8.0) / (stats.timeLastRxPacket.GetSeconds() - stats.timeFirstTxPacket.GetSeconds()) / 1024 / 1024;
            flowCount++;
        }
    }

    if (flowCount > 0) {
        averageThroughput /= flowCount;
        NS_LOG_UNCOND("Average TCP Throughput: " << averageThroughput << " Mbps");
    } else {
        NS_LOG_UNCOND("No TCP traffic received.");
    }

    Simulator::Destroy();
    return 0;
}