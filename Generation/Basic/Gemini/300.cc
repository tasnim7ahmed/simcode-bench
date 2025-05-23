#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/zigbee-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    double simTime = 10.0;
    uint32_t nEndDevices = 9; // Total 10 nodes (1 coordinator + 9 end devices)
    uint32_t packetSize = 128;
    double interPacketInterval = 0.5; // 500ms

    CommandLine cmd(__FILE__);
    cmd.AddValue("simTime", "Total simulation time in seconds", simTime);
    cmd.AddValue("nEndDevices", "Number of end devices (total nodes will be nEndDevices + 1)", nEndDevices);
    cmd.AddValue("packetSize", "Size of UDP packets in bytes", packetSize);
    cmd.AddValue("interPacketInterval", "Time interval between packets in seconds", interPacketInterval);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(nEndDevices + 1);

    Ptr<Node> coordinator = nodes.Get(0);
    NodeContainer endDevices;
    for (uint32_t i = 1; i <= nEndDevices; ++i)
    {
        endDevices.Add(nodes.Get(i));
    }

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", StringValue("0|100|0|100"),
                              "Speed", StringValue("ns3::UniformRandomVariable[Min=0.5|Max=1.5]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"));
    mobility.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    ZigBeeHelper zigbee;
    zigbee.SetPhyMode(ZigBeeHelper::PhyMode::HR_DSSS_CHIP_RATE_250_KBPS);
    zigbee.SetMacType(ZigBeeHelper::MacType::MAC_802_15_4_PRO);

    NetDeviceContainer coordinatorDevs = zigbee.InstallCoordinator(coordinator);
    NetDeviceContainer endDeviceDevs = zigbee.InstallEndDevice(endDevices);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer coordinatorInterface = ipv4.Assign(coordinatorDevs);
    Ipv4InterfaceContainer endDeviceInterfaces = ipv4.Assign(endDeviceDevs);

    Ipv4Address coordinatorIp = coordinatorInterface.GetAddress(0);
    uint16_t port = 9;

    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(coordinator);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime + 1.0));

    UdpEchoClientHelper echoClient(coordinatorIp, port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(static_cast<uint32_t>(simTime / interPacketInterval) + 1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(interPacketInterval)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < nEndDevices; ++i)
    {
        clientApps.Add(echoClient.Install(endDevices.Get(i)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simTime + 1.0));

    zigbee.EnablePcapAll("zigbee-wsn");

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}