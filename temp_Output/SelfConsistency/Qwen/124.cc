#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VlanHubRoutingSimulation");

int main(int argc, char *argv[]) {
    bool tracing = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.Parse(argc, argv);

    NS_LOG_INFO("Creating nodes.");
    NodeContainer hub;
    hub.Create(1);

    NodeContainer vlanNodes;
    vlanNodes.Create(3);

    NodeContainer allNodes;
    allNodes.Add(hub);
    allNodes.Add(vlanNodes);

    NS_LOG_INFO("Creating CSMA channels.");
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(100e6)));
    csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(0.01)));

    NetDeviceContainer hubDevices;
    NetDeviceContainer vlanDevices[3];

    for (int i = 0; i < 3; ++i) {
        std::ostringstream subnetName;
        subnetName << "csma" << i;
        csma.SetDeviceAttribute("Mtu", UintegerValue(1500));
        NetDeviceContainer devices = csma.Install(NodeContainer(hub.Get(0), vlanNodes.Get(i)));
        hubDevices.Add(devices.Get(0));
        vlanDevices[i].Add(devices.Get(1));
    }

    NS_LOG_INFO("Assigning IP addresses.");
    InternetStackHelper internet;
    internet.Install(allNodes);

    Ipv4AddressHelper ipv4;
    Ipv4InterfaceContainer hubInterfaces[3];
    Ipv4InterfaceContainer vlanInterfaces[3];

    for (int i = 0; i < 3; ++i) {
        std::ostringstream subnet;
        subnet << "192.168." << i << ".0";
        ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(NetDeviceContainer(hubDevices.Get(i), vlanDevices[i].Get(0)));
        hubInterfaces[i] = interfaces.Get(0);
        vlanInterfaces[i] = interfaces.Get(1);
    }

    NS_LOG_INFO("Setting up global routing.");
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    NS_LOG_INFO("Creating applications.");

    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);

    ApplicationContainer serverApps = echoServer.Install(vlanNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(vlanInterfaces[0].GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(vlanNodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    if (tracing) {
        NS_LOG_INFO("Enabling pcap tracing.");
        csma.EnablePcapAll("vlan_hub_routing");
    }

    NS_LOG_INFO("Running simulation.");
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Simulation completed.");
    
    return 0;
}