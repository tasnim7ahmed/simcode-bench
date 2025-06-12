#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create LTE helper components
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetSchedulerType("ns3::PfFfMacScheduler");

    // Create eNodeB and UE nodes
    NodeContainer enbNodes;
    NodeContainer ueNodes;
    enbNodes.Create(1);
    ueNodes.Create(3);

    // Install LTE devices
    NetDeviceContainer enbDevs;
    NetDeviceContainer ueDevs;

    enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Attach UEs to eNodeB
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Install the Internet stack on UEs and eNodeB
    InternetStackHelper internet;
    internet.Install(enbNodes);
    internet.Install(ueNodes);

    // Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = lteHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Set up UDP Echo Server on eNodeB
    uint16_t port = 9;  // UDP port number
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(enbNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Configure UDP clients on UEs
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        UdpEchoClientHelper echoClient(ueIpIface.GetAddress(i), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = echoClient.Install(ueNodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
    }

    // Set up mobility models
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(ueNodes);

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}