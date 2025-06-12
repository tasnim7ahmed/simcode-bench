#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridTopologySimulation");

int main(int argc, char *argv[]) {
    uint32_t tcpClients = 5;
    uint32_t meshNodes = 6;
    double simulationTime = 10.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("tcpClients", "Number of TCP clients in star topology", tcpClients);
    cmd.AddValue("meshNodes", "Number of UDP nodes in mesh topology", meshNodes);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    NodeContainer tcpServerNode;
    tcpServerNode.Create(1);

    NodeContainer tcpClientNodes;
    tcpClientNodes.Create(tcpClients);

    NodeContainer meshNodesContainer;
    meshNodesContainer.Create(meshNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer tcpServerDev;
    InternetStackHelper stack;
    stack.Install(tcpServerNode);
    stack.Install(tcpClientNodes);

    for (uint32_t i = 0; i < tcpClients; ++i) {
        PointToPointHelper tempP2P = p2p;
        NetDeviceContainer devices = tempP2P.Install(tcpServerNode.Get(0), tcpClientNodes.Get(i));
        tcpServerDev.Add(devices.Get(0));
    }

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer meshDevices = wifi.Install(wifiPhy, wifiMac, meshNodesContainer);

    stack.Install(meshNodesContainer);

    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer tcpServerInterfaces;
    for (uint32_t i = 0; i < tcpServerDev.GetN(); ++i) {
        Ipv4InterfaceContainer intf = address.Assign(NetDeviceContainer(tcpServerDev.Get(i)));
        tcpServerInterfaces.Add(intf);
        address.NewNetwork();
    }

    for (uint32_t i = 0; i < tcpClients; ++i) {
        Ipv4InterfaceContainer clientIf = address.Assign(tcpClientNodes.Get(i)->GetDevice(0));
        address.NewNetwork();
    }

    address.SetBase("10.2.1.0", "255.255.255.0");
    Ipv4InterfaceContainer meshInterfaces = address.Assign(meshDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t tcpPort = 5000;
    UdpEchoServerHelper udpServer(tcpPort);
    ApplicationContainer udpServerApps = udpServer.Install(meshNodesContainer);
    udpServerApps.Start(Seconds(1.0));
    udpServerApps.Stop(Seconds(simulationTime));

    for (uint32_t i = 0; i < meshNodes; ++i) {
        for (uint32_t j = 0; j < meshNodes; ++j) {
            if (i != j) {
                UdpEchoClientHelper udpClient(meshInterfaces.GetAddress(j), tcpPort);
                udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
                udpClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
                udpClient.SetAttribute("PacketSize", UintegerValue(1024));

                ApplicationContainer clientApp = udpClient.Install(meshNodesContainer.Get(i));
                clientApp.Start(Seconds(2.0));
                clientApp.Stop(Seconds(simulationTime - 1));
            }
        }
    }

    uint16_t tcpEchoPort = 9;
    TcpEchoServerHelper tcpServer(tcpEchoPort);
    ApplicationContainer tcpServerApps = tcpServer.Install(tcpServerNode);
    tcpServerApps.Start(Seconds(1.0));
    tcpServerApps.Stop(Seconds(simulationTime));

    for (uint32_t i = 0; i < tcpClients; ++i) {
        Address serverAddress = tcpServerInterfaces.GetAddress(0, 0);
        TcpEchoClientHelper tcpClient(serverAddress, tcpEchoPort);
        tcpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        tcpClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        tcpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = tcpClient.Install(tcpClientNodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(simulationTime - 1));
    }

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(NodeContainer::GetGlobal());

    AnimationInterface anim("hybrid_topology.xml");
    anim.SetMobilityPollInterval(Seconds(1));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    monitor->SerializeToXmlFile("flowmonitor_output.xml", false, false);

    Simulator::Destroy();
    return 0;
}