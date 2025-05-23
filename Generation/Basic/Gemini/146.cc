#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

int main(int argc, char *argv[])
{
    // 1. Create four nodes
    NodeContainer nodes;
    nodes.Create(4);

    // 2. Assign static positions in a 2x2 grid layout
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    nodes.Get(0)->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));
    nodes.Get(1)->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(10.0, 0.0, 0.0));
    nodes.Get(2)->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(0.0, 10.0, 0.0));
    nodes.Get(3)->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(10.0, 10.0, 0.0));

    // 3. Configure Wi-Fi mesh network
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211a); // Recommended for mesh

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
    
    MeshHelper mesh;
    mesh.SetNumberOfInterfaces(1); // One Wi-Fi interface per node
    mesh.SetSpreadInterfaceChannels(false); // All interfaces use the same channel
    
    // Set the MAC type to AdhocWifiMac for mesh
    mesh.SetMacType("ns3::AdhocWifiMac", "Ssid", Ssid("ns-3-mesh"));

    NetDeviceContainer netDevices = mesh.Install(wifiPhy, wifiMac, nodes);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(netDevices);

    // 4. UDP traffic generation (ring topology: N0->N1, N1->N2, N2->N3, N3->N0)
    uint16_t port = 9; // Discard port

    // Packet Sink (server) applications
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        sinkApps.Add(sinkHelper.Install(nodes.Get(i)));
    }
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(15.0));

    // OnOff (client) applications
    OnOffHelper onoffClient0to1("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port)); // Node 0 -> Node 1 (10.1.1.2)
    onoffClient0to1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoffClient0to1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    onoffClient0to1.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoffClient0to1.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps;
    clientApps.Add(onoffClient0to1.Install(nodes.Get(0)));

    OnOffHelper onoffClient1to2("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(2), port)); // Node 1 -> Node 2 (10.1.1.3)
    onoffClient1to2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoffClient1to2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    onoffClient1to2.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoffClient1to2.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps.Add(onoffClient1to2.Install(nodes.Get(1)));

    OnOffHelper onoffClient2to3("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(3), port)); // Node 2 -> Node 3 (10.1.1.4)
    onoffClient2to3.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoffClient2to3.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    onoffClient2to3.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoffClient2to3.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps.Add(onoffClient2to3.Install(nodes.Get(2)));

    OnOffHelper onoffClient3to0("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(0), port)); // Node 3 -> Node 0 (10.1.1.1)
    onoffClient3to0.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoffClient3to0.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    onoffClient3to0.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoffClient3to0.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps.Add(onoffClient3to0.Install(nodes.Get(3)));

    clientApps.Start(Seconds(1.0)); // Start client traffic after 1 second
    clientApps.Stop(Seconds(14.0)); // Stop client traffic 1 second before simulation ends

    // 5. Enable PCAP tracing
    wifiPhy.EnablePcapAll("mesh-wifi-trace");

    // 6. Record basic statistics using FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // 7. Set simulation duration
    Simulator::Stop(Seconds(15.0));
    Simulator::Run();

    // Export FlowMonitor results to an XML file
    monitor->SerializeToXmlFile("mesh-wifi-flowmon.xml", true, true);

    Simulator::Destroy();

    return 0;
}