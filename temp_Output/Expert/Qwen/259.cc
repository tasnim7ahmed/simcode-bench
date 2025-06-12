#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/mmwave-helper.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MmWave5GSimulation");

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create gNB and UEs
    NodeContainer gnb;
    gnb.Create(1);

    NodeContainer ues;
    ues.Create(2);

    // Mobility for UEs: Random Walk
    MobilityHelper mobilityUe;
    mobilityUe.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)),
                                "Distance", DoubleValue(1.0));
    mobilityUe.Install(ues);

    // Mobility for gNB: stationary
    MobilityHelper mobilityGnb;
    mobilityGnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityGnb.Install(gnb);

    // Install mmWave devices
    Ptr<MmWaveHelper> mmWave = CreateObject<MmWaveHelper>();
    mmWave->SetSchedulerType("ns3::MmWaveFlexTtiMacScheduler");

    NetDeviceContainer gnbDevs;
    NetDeviceContainer ueDevs;

    gnbDevs = mmWave->InstallGnbDevice(gnb);
    ueDevs = mmWave->InstallUeDevice(ues);

    // Attach UEs to gNB
    for (uint32_t i = 0; i < ueDevs.GetN(); ++i) {
        mmWave->Attach(ueDevs.Get(i), gnbDevs.Get(0));
    }

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(ues);
    internet.Install(gnb);

    // Assign IPv4 addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ueInterfaces;
    Ipv4InterfaceContainer gnbInterfaces;

    gnbInterfaces = ipv4.Assign(gnbDevs);
    ueInterfaces = ipv4.Assign(ueDevs);

    // Set up applications
    uint16_t port = 9;

    // Server on UE 1 (index 1)
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(ues.Get(1));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Client on UE 0 (index 0), send 5 packets of 1024 bytes each
    UdpEchoClientHelper echoClient(ueInterfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(ues.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Simulation setup
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}