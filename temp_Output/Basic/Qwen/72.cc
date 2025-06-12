#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/bridge-module.h"
#include "ns3/global-router-interface.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TwoLanTopologyWithBridges");

int main(int argc, char *argv[]) {
    std::string lanLinkType = "100Mbps";
    std::string wanDataRate = "5Mbps";
    std::string wanDelay = "50ms";

    CommandLine cmd(__FILE__);
    cmd.AddValue("lan-link-type", "LAN link type: 100Mbps or 10Mbps", lanLinkType);
    cmd.AddValue("wan-data-rate", "WAN data rate (e.g., 5Mbps)", wanDataRate);
    cmd.AddValue("wan-delay", "WAN delay (e.g., 50ms)", wanDelay);
    cmd.Parse(argc, argv);

    bool use100Mbps = (lanLinkType == "100Mbps");

    NodeContainer topLanSwitches;
    topLanSwitches.Create(3);

    NodeContainer bottomLanSwitches;
    bottomLanSwitches.Create(3);

    NodeContainer trBr;
    trBr.Create(2);

    NodeContainer topHosts;
    topHosts.Create(2);

    NodeContainer bottomHosts;
    bottomHosts.Create(2);

    CsmaHelper csma;
    if (use100Mbps) {
        csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    } else {
        csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
    }
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(wanDataRate));
    p2p.SetChannelAttribute("Delay", StringValue(wanDelay));

    NetDeviceContainer topSwitchDevices;
    for (uint32_t i = 0; i < topLanSwitches.GetN(); ++i) {
        for (uint32_t j = i + 1; j < topLanSwitches.GetN(); ++j) {
            NetDeviceContainer devs = csma.Install(NodeContainer(topLanSwitches.Get(i), topLanSwitches.Get(j)));
            topSwitchDevices.Add(devs);
        }
    }

    NetDeviceContainer bottomSwitchDevices;
    for (uint32_t i = 0; i < bottomLanSwitches.GetN(); ++i) {
        for (uint32_t j = i + 1; j < bottomLanSwitches.GetN(); ++j) {
            NetDeviceContainer devs = csma.Install(NodeContainer(bottomLanSwitches.Get(i), bottomLanSwitches.Get(j)));
            bottomSwitchDevices.Add(devs);
        }
    }

    BridgeHelper bridge;

    for (uint32_t i = 0; i < topLanSwitches.GetN(); ++i) {
        Ptr<Node> switchNode = topLanSwitches.Get(i);
        NetDeviceContainer connectedDevs;
        for (uint32_t j = 0; j < topSwitchDevices.GetN(); ++j) {
            if (topSwitchDevices.Get(j)->GetNode() == switchNode ||
                topSwitchDevices.Get(j)->GetRemote()->GetNode() == switchNode) {
                connectedDevs.Add(topSwitchDevices.Get(j));
            }
        }
        bridge.Install(switchNode, connectedDevs);
    }

    for (uint32_t i = 0; i < bottomLanSwitches.GetN(); ++i) {
        Ptr<Node> switchNode = bottomLanSwitches.Get(i);
        NetDeviceContainer connectedDevs;
        for (uint32_t j = 0; j < bottomSwitchDevices.GetN(); ++j) {
            if (bottomSwitchDevices.Get(j)->GetNode() == switchNode ||
                bottomSwitchDevices.Get(j)->GetRemote()->GetNode() == switchNode) {
                connectedDevs.Add(bottomSwitchDevices.Get(j));
            }
        }
        bridge.Install(switchNode, connectedDevs);
    }

    NetDeviceContainer topHostDevices;
    CsmaHelper hostCsma;
    hostCsma.SetChannelAttribute("DataRate", DataRateValue(use100Mbps ? DataRate("100Mbps") : DataRate("10Mbps")));
    hostCsma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    topHostDevices.Add(hostCsma.Install(topHosts.Get(0), topLanSwitches.Get(0)));
    topHostDevices.Add(hostCsma.Install(topHosts.Get(1), topLanSwitches.Get(1)));

    NetDeviceContainer bottomHostDevices;
    bottomHostDevices.Add(hostCsma.Install(bottomHosts.Get(0), bottomLanSwitches.Get(0)));
    bottomHostDevices.Add(hostCsma.Install(bottomHosts.Get(1), bottomLanSwitches.Get(1)));

    NetDeviceContainer wanDevices = p2p.Install(trBr);

    InternetStackHelper internet;
    internet.InstallAll();

    Ipv4AddressHelper ipv4;

    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    ipv4.Assign(topHostDevices);
    Ipv4InterfaceContainer topRouterInterfaces;
    topRouterInterfaces.Add(ipv4.Assign(wanDevices.Get(0)));

    ipv4.SetBase("192.168.2.0", "255.255.255.0");
    ipv4.Assign(bottomHostDevices);
    Ipv4InterfaceContainer bottomRouterInterfaces;
    bottomRouterInterfaces.Add(ipv4.Assign(wanDevices.Get(1)));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t echoPort = 9;

    UdpEchoServerHelper echoServer(echoPort);

    ApplicationContainer serverApps = echoServer.Install(bottomHosts.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    serverApps = echoServer.Install(topHosts.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient;
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    echoClient.SetAttribute("RemoteAddress", Ipv4AddressValue("192.168.2.1"));
    echoClient.SetAttribute("RemotePort", UintegerValue(echoPort));
    ApplicationContainer clientApps = echoClient.Install(topHosts.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    echoClient.SetAttribute("RemoteAddress", Ipv4AddressValue("192.168.1.3"));
    echoClient.SetAttribute("RemotePort", UintegerValue(echoPort));
    clientApps = echoClient.Install(bottomHosts.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}