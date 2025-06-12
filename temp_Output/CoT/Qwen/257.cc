#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    double simTime = 10.0;
    Config::SetDefault("ns3::LteHelper::UseRlcSm", BooleanValue(true));
    Config::SetDefault("ns3::UdpClient::MaxPackets", UintegerValue(2));
    Config::SetDefault("ns3::UdpClient::Interval", TimeValue(MilliSeconds(100)));
    Config::SetDefault("ns3::UdpClient::PacketSize", UintegerValue(512));

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    lteHelper->SetAttribute("PathlossModel", StringValue("ns3::FriisSpectrumPropagationLossModel"));

    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(2);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    epcHelper->AddEnb(enbNodes.Get(0), enbDevs.Get(0));
    uint16_t cellId = 1;
    uint8_t taid = 1;
    epcHelper->AddUe(ueDevs.Get(0), cellId, taid);
    epcHelper->AddUe(ueDevs.Get(1), cellId, taid);

    InternetStackHelper internet;
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(ueDevs);

    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    uint16_t dlPort = 9;
    uint16_t ulPort = 10;
    ApplicationContainer serverApps;
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u) {
        if (u == 1) {
            PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), dlPort));
            serverApps.Add(sinkHelper.Install(ueNodes.Get(u)));
        }
    }
    serverApps.Start(Seconds(0.01));
    serverApps.Stop(Seconds(simTime));

    for (uint32_t u = 0; u < ueNodes.GetN(); ++u) {
        if (u == 0) {
            InetSocketAddress dstAddr = InetSocketAddress(ueIpIface.GetAddress(1), dlPort);
            UdpClientHelper client(dstAddr);
            ApplicationContainer clientApp = client.Install(ueNodes.Get(u));
            clientApp.Start(Seconds(1.0));
            clientApp.Stop(Seconds(simTime));
        }
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}