#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/point-to-point-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create LTE helper
    LteHelper lteHelper;

    // Create eNodeB node
    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Create UE node
    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Add eNodeB and UE to LTE helper
    NetDeviceContainer enbDevs;
    enbDevs = lteHelper.InstallEnbDevice(enbNodes);

    NetDeviceContainer ueDevs;
    ueDevs = lteHelper.InstallUeDevice(ueNodes);

    // Mobility model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Mode", StringValue("Time"),
                              "Time", StringValue("1s"),
                              "Speed", StringValue("1.0"));
    mobility.Install(ueNodes);

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(enbNodes);

    Simulator::Schedule(Seconds(0.001), [&]() {
        Ptr<MobilityModel> mob = enbNodes.Get(0)->GetObject<MobilityModel>();
        mob->SetPosition(Vector(25.0, 25.0, 0.0));
    });

    // Install internet stack
    InternetStackHelper internet;
    internet.Install(enbNodes);
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIpIface;
    Ipv4InterfaceContainer enbIpIface;

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ueIpIface = ipv4.Assign(ueDevs);
    enbIpIface = ipv4.Assign(enbDevs);

    // Set up UDP server on eNodeB
    uint16_t port = 80;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(enbNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP client on UE
    UdpEchoClientHelper echoClient(enbIpIface.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = echoClient.Install(ueNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Attach UE to eNodeB
    lteHelper.Attach(ueDevs, enbDevs.Get(0));

    // Activate a data radio bearer
    enum EpsBearer::Qci q = EpsBearer::GBR_QCI_DEFAULT;
    EpsBearer bearer(q);
    lteHelper.ActivateDataRadioBearer(ueNodes, bearer, enbDevs.Get(0));

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    Simulator::Destroy();
    return 0;
}