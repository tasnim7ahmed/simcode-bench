#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologySimulation");

int main(int argc, char *argv[]) {
    uint32_t numSpokes = 8;
    Time simulationTime = Seconds(10);

    NodeContainer hub;
    hub.Create(1);

    NodeContainer spokes;
    spokes.Create(numSpokes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("14kbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer hubDevices;
    NetDeviceContainer spokeDevices;

    for (uint32_t i = 0; i < numSpokes; ++i) {
        NetDeviceContainer devices = p2p.Install(hub.Get(0), spokes.Get(i));
        hubDevices.Add(devices.Get(0));
        spokeDevices.Add(devices.Get(1));
    }

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer hubInterfaces;
    Ipv4InterfaceContainer spokeInterfaces;

    for (uint32_t i = 0; i < numSpokes; ++i) {
        std::ostringstream subnet;
        subnet << "10.0." << i << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(hubDevices.Get(i), spokeDevices.Get(i)));
        hubInterfaces.Add(interfaces.Get(0));
        spokeInterfaces.Add(interfaces.Get(1));
    }

    // Packet sink on the hub to receive TCP traffic on port 50000
    uint16_t sinkPort = 50000;
    Address sinkAddress(InetSocketAddress(hubInterfaces.GetAddress(0), sinkPort));

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(hub.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(simulationTime);

    // Configure OnOff applications on each spoke node to send to the hub
    OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddress);
    onoff.SetConstantRate(DataRate("14kbps"), 137);
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer clientApps;

    for (uint32_t i = 0; i < numSpokes; ++i) {
        ApplicationContainer app = onoff.Install(spokes.Get(i));
        app.Start(Seconds(1.0));
        app.Stop(simulationTime);
        clientApps.Add(app);
    }

    // Enable static global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Enable pcap tracing for all devices
    p2p.EnablePcapAll("star_topology");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}