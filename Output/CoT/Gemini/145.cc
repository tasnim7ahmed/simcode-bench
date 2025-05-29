#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MeshExample");

int main(int argc, char *argv[]) {
    bool enablePcap = true;
    bool enableFlowMonitor = true;

    CommandLine cmd;
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
    cmd.AddValue("enableFlowMonitor", "Enable Flow Monitor", enableFlowMonitor);
    cmd.Parse(argc, argv);

    NodeContainer meshNodes;
    meshNodes.Create(3);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211s);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMeshHelper mesh;
    mesh.SetMacType("RandomStart");

    NetDeviceContainer meshDevices = mesh.Install(wifiPhy, meshNodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(meshNodes);

    InternetStackHelper internet;
    internet.Install(meshNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(meshDevices);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(meshNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(meshNodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    clientApps = echoClient.Install(meshNodes.Get(2));
    clientApps.Start(Seconds(3.0));
    clientApps.Stop(Seconds(10.0));

    if (enablePcap) {
        wifiPhy.EnablePcapAll("mesh-example");
    }

    FlowMonitorHelper flowmon;
    if (enableFlowMonitor) {
        flowmon.InstallAll();
    }

    Simulator::Stop(Seconds(10.0));

    AnimationInterface anim("mesh-animation.xml");

    Simulator::Run();

    if (enableFlowMonitor) {
        flowmon.SerializeToXmlFile("mesh-flowmon.xml", false, true);
    }

    Simulator::Destroy();
    return 0;
}