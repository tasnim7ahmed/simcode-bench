#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/dv-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CountToInfinitySimulation");

int main(int argc, char *argv[]) {
    bool verbose = true;
    if (verbose) {
        LogComponentEnable("CountToInfinitySimulation", LOG_LEVEL_INFO);
    }

    // Create nodes for routers A, B, C
    NodeContainer routers;
    routers.Create(3);

    // Create point-to-point links between A-B and B-C
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devAB, devBC, devBA, devCB;
    devAB = p2p.Install(routers.Get(0), routers.Get(1));  // A <-> B
    devBC = p2p.Install(routers.Get(1), routers.Get(2));  // B <-> C

    // Install Internet stack with Distance Vector routing
    DvRoutingHelper dv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(dv);
    stack.Install(routers);

    // Assign IP addresses to interfaces
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ipAB = ipv4.Assign(devAB);
    ipv4.NewNetwork();
    Ipv4InterfaceContainer ipBC = ipv4.Assign(devBC);

    // Enable packet tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("count-to-infinity.tr"));
    p2p.EnablePcapAll("count-to-infinity");

    // Start simulation
    Simulator::Schedule(Seconds(1.0), &Ipv4StaticRouting::SetDefaultRouteInAllNodes, Ipv4StaticRouting::GetInstance());
    Simulator::Schedule(Seconds(2.0), []() {
        std::cout << "Breaking link between B and C at 2.0s" << std::endl;
        Ptr<Ipv4> ipv4B = routers.Get(1)->GetObject<Ipv4>();
        Ptr<Ipv4> ipv4C = routers.Get(2)->GetObject<Ipv4>();
        uint32_t interfaceB = ipv4B->GetInterfaceForDevice(devBC.Get(0));
        uint32_t interfaceC = ipv4C->GetInterfaceForDevice(devBC.Get(1));

        ipv4B->SetDown(interfaceB);
        ipv4C->SetDown(interfaceC);
    });

    // Stop simulation after 30 seconds
    Simulator::Stop(Seconds(30.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}