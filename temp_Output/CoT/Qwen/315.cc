#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/config-store-module.h"
#include "ns3/nr-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    double simDuration = 10.0;
    uint32_t numUe = 1;
    uint32_t numGnb = 1;
    uint32_t packetCount = 1000;
    uint32_t packetSize = 1024;
    double interval = 0.01;
    uint16_t port = 9;

    Config::SetDefault("ns3::NrUePhy::Numerology", UintegerValue(1));
    Config::SetDefault("ns3::NrGnbPhy::Numerology", UintegerValue(1));

    NodeContainer ueNodes;
    NodeContainer gnbNodes;
    ueNodes.Create(numUe);
    gnbNodes.Create(numGnb);

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));   // gNB
    positionAlloc->Add(Vector(10.0, 0.0, 0.0));   // UE

    MobilityHelper mobility;
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gnbNodes);
    mobility.Install(ueNodes);

    NrHelper nrHelper;
    nrHelper.SetSchedulerTypeId("ns3::nr::bwp::NGBR_SCHEDULER");

    NetDeviceContainer enbNetDev;
    NetDeviceContainer ueNetDev;

    SfnSf sfnsf{0, 0, 0};
    BandwidthPartInfoPtrVector allBwps = nrHelper.GetBwps();

    enbNetDev = nrHelper.InstallGnbDevice(gnbNodes, allBwps);
    ueNetDev = nrHelper.InstallUeDevice(ueNodes, allBwps);

    nrHelper.Attach(ueNetDev, enbNetDev.Get(0));

    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(gnbNodes);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer ueIpIface;
    Ipv4InterfaceContainer gnbIpIface;
    ueIpIface = ipv4h.Assign(ueNetDev);
    gnbIpIface = ipv4h.Assign(enbNetDev);

    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(ueNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simDuration));

    UdpClientHelper client(ueIpIface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(packetCount));
    client.SetAttribute("Interval", TimeValue(Seconds(interval)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp = client.Install(gnbNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(simDuration));

    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}