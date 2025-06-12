#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create LTE Helper
    LteHelper lteHelper;

    // Create eNodeB and UEs
    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(3);

    // Create LTE devices
    NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice(ueNodes);

    // Install the Internet stack
    InternetStackHelper internet;
    internet.Install(enbNodes);
    internet.Install(ueNodes);

    // Assign IP addresses
    Ipv4InterfaceContainer enbIpIface;
    Ipv4InterfaceContainer ueIpIface;
    Ipv4AddressHelper ip;
    ip.SetBase("10.1.1.0", "255.255.255.0");
    enbIpIface = ip.Assign(enbLteDevs);
    ueIpIface = ip.Assign(ueLteDevs);

    // Attach UEs to eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        lteHelper.Attach(ueLteDevs.Get(i), enbLteDevs.Get(0));
    }

    // Set up UDP Echo Server on eNodeB
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(enbNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up UDP Echo Clients on UEs
    UdpEchoClientHelper echoClient(enbIpIface.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        clientApps.Add(echoClient.Install(ueNodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Mobility Model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Mode", StringValue("Time"),
                              "Time", StringValue("1s"),
                              "Speed", StringValue("1.0m/s"),
                              "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
    mobility.Install(ueNodes);
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    mobility.Install(enbNodes);

    // Set eNodeB initial position
    Ptr<ConstantPositionMobilityModel> enbMobility = enbNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
    if (enbMobility) {
        enbMobility->SetPosition(Vector(25.0, 25.0, 0.0));
    }

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    Simulator::Destroy();
    return 0;
}