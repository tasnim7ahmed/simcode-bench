#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create Nodes: one gNB and two UEs
    NodeContainer gnbNodes;
    gnbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(2);

    // Configure Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gnbNodes);

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                  "Bounds", RectangleValue(Rectangle(-100, 100, -100, 100)));
    mobility.Install(ueNodes);


    // Install NR net devices
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    NetDeviceContainer gnbDevs = nrHelper->InstallGnbDevice(gnbNodes);
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(gnbNodes);
    stack.Install(ueNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer gnbIpIface = address.Assign(gnbDevs);
    Ipv4InterfaceContainer ueIpIface = address.Assign(ueDevs);

    // Set up the UDP Echo Server on UE2
    uint16_t port = 9;  // UDP port number
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(ueNodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up the UDP Echo Client on UE1
    UdpEchoClientHelper echoClient(ueIpIface.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(ueNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Attach UEs to the gNB
    nrHelper->Attach(ueDevs.Get(0), gnbDevs.Get(0));
    nrHelper->Attach(ueDevs.Get(1), gnbDevs.Get(0));

    // Activate default EPS bearer
    enum EpsBearer::Qci q = EpsBearer::GBR_CONV_VOICE;
    EpsBearer bearer(q);
    nrHelper->ActivateDedicatedEpsBearer(ueDevs, bearer);


    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}