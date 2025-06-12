#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologySimulation");

int main(int argc, char *argv[]) {
    uint32_t nSpokes = 8;
    Time simulationTime = Seconds(10);

    NodeContainer hub;
    hub.Create(1);

    NodeContainer spokes;
    spokes.Create(nSpokes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("14kbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer hubDevices;
    NetDeviceContainer spokeDevices;

    for (uint32_t i = 0; i < nSpokes; ++i) {
        NetDeviceContainer link = p2p.Install(hub.Get(0), spokes.Get(i));
        hubDevices.Add(link.Get(0));
        spokeDevices.Add(link.Get(1));
    }

    InternetStackHelper internet;
    internet.InstallAll();

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer hubInterfaces;
    Ipv4InterfaceContainer spokeInterfaces;

    for (uint32_t i = 0; i < nSpokes; ++i) {
        Ipv4InterfaceContainer interfaces = ipv4.Assign(NetDeviceContainer(hubDevices.Get(i), spokeDevices.Get(i)));
        hubInterfaces.Add(interfaces.Get(0));
        spokeInterfaces.Add(interfaces.Get(1));
        ipv4.NewNetwork();
    }

    uint16_t sinkPort = 50000;

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(hub.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(simulationTime);

    OnOffHelper onOffHelper("ns3::TcpSocketFactory", Address());
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(137));
    onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("14kbps")));

    ApplicationContainer clientApps;

    for (uint32_t i = 0; i < nSpokes; ++i) {
        Address sinkAddress(InetSocketAddress(hubInterfaces.GetAddress(0), sinkPort));
        onOffHelper.SetAttribute("Remote", AddressValue(sinkAddress));
        ApplicationContainer app = onOffHelper.Install(spokes.Get(i));
        app.Start(Seconds(1.0));
        app.Stop(simulationTime);
        clientApps.Add(app);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    for (uint32_t i = 0; i < hubDevices.GetN(); ++i) {
        p2p.EnablePcapAll("hub-device-" + std::to_string(i));
    }

    for (uint32_t i = 0; i < spokeDevices.GetN(); ++i) {
        p2p.EnablePcapAll("spoke-device-" + std::to_string(i));
    }

    Simulator::Stop(simulationTime);
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}