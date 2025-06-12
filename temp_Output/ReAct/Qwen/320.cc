#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    double simDuration = 10.0;
    uint32_t numUeNodes = 3;
    uint16_t serverPort = 9;
    uint32_t packetSize = 1024;
    uint32_t numPackets = 1000;
    double interval = 0.01;

    Config::SetDefault("ns3::LteHelper::UseIdealRrc", BooleanValue(true));

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetSchedulerType("ns3::RrFfMacScheduler");

    NodeContainer enbNodes;
    enbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(numUeNodes);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    NetDeviceContainer enbDevs;
    enbDevs = lteHelper->InstallEnbDevice(enbNodes);

    NetDeviceContainer ueDevs;
    ueDevs = lteHelper->InstallUeDevice(ueNodes);

    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    Ipv4AddressHelper ipv4Addr;
    ipv4Addr.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer enbIfaces;
    enbIfaces = lteHelper->AssignIpv4AddressesToEpcEnb(enbDevs);

    Ipv4InterfaceContainer ueIfaces;
    ueIfaces = lteHelper->AssignIpv4AddressForUe(ueDevs);

    Ipv4Address serverAddr = ueIfaces.GetAddress(0, 0);

    UdpServerHelper server(serverPort);
    ApplicationContainer serverApps = server.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simDuration));

    UdpClientHelper clientHelper(serverAddr, serverPort);
    clientHelper.SetAttribute("MaxPackets", UintegerValue(numPackets));
    clientHelper.SetAttribute("Interval", TimeValue(Seconds(interval)));
    clientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < numUeNodes; ++i) {
        clientApps.Add(clientHelper.Install(ueNodes.Get(i)));
    }

    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simDuration));

    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}