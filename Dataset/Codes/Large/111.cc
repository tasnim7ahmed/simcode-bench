// Network topology
// //
// //             n0   R    n1
// //             |    _    |
// //             ====|_|====
// //                router
// // - R sends RA to n0's subnet (2001:1::/64);
// // - R sends RA to n1's subnet (2001:2::/64);
// // - n0 ping n1.
// //
// // - Tracing of queues and packet receptions to file "radvd.tr"

#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/internet-module.h"
#include "ns3/radvd-interface.h"
#include "ns3/radvd-prefix.h"
#include "ns3/radvd.h"

#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RadvdExample");

int
main(int argc, char** argv)
{
    bool verbose = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "turn on log components", verbose);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_ALL);
        LogComponentEnable("Ipv6RawSocketImpl", LOG_LEVEL_ALL);
        LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_ALL);
        LogComponentEnable("Ipv6StaticRouting", LOG_LEVEL_ALL);
        LogComponentEnable("Ipv6Interface", LOG_LEVEL_ALL);
        LogComponentEnable("RadvdApplication", LOG_LEVEL_ALL);
        LogComponentEnable("Ping", LOG_LEVEL_ALL);
    }

    NS_LOG_INFO("Create nodes.");
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();

    NodeContainer net1(n0, r);
    NodeContainer net2(r, n1);
    NodeContainer all(n0, r, n1);

    NS_LOG_INFO("Create IPv6 Internet Stack");
    InternetStackHelper internetv6;
    internetv6.Install(all);

    NS_LOG_INFO("Create channels.");
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer d1 = csma.Install(net1); /* n0 - R */
    NetDeviceContainer d2 = csma.Install(net2); /* R - n1 */

    NS_LOG_INFO("Create networks and assign IPv6 Addresses.");
    Ipv6AddressHelper ipv6;

    /* first subnet */
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    NetDeviceContainer tmp;
    tmp.Add(d1.Get(0));                                           /* n0 */
    Ipv6InterfaceContainer iic1 = ipv6.AssignWithoutAddress(tmp); /* n0 interface */

    NetDeviceContainer tmp2;
    tmp2.Add(d1.Get(1)); /* R */
    Ipv6InterfaceContainer iicr1 =
        ipv6.Assign(tmp2); /* R interface to the first subnet is just statically assigned */
    iicr1.SetForwarding(0, true);
    iic1.Add(iicr1);

    /* second subnet R - n1 */
    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    NetDeviceContainer tmp3;
    tmp3.Add(d2.Get(0));                              /* R */
    Ipv6InterfaceContainer iicr2 = ipv6.Assign(tmp3); /* R interface */
    iicr2.SetForwarding(0, true);

    NetDeviceContainer tmp4;
    tmp4.Add(d2.Get(1)); /* n1 */
    Ipv6InterfaceContainer iic2 = ipv6.AssignWithoutAddress(tmp4);
    iic2.Add(iicr2);

    /* radvd configuration */
    RadvdHelper radvdHelper;

    /* R interface (n0 - R) */
    /* n0 will receive unsolicited (periodic) RA */
    radvdHelper.AddAnnouncedPrefix(iic1.GetInterfaceIndex(1), Ipv6Address("2001:1::0"), 64);

    /* R interface (R - n1) */
    /* n1 will have to use RS, as RA are not sent automatically */
    radvdHelper.AddAnnouncedPrefix(iic2.GetInterfaceIndex(1), Ipv6Address("2001:2::0"), 64);
    radvdHelper.GetRadvdInterface(iic2.GetInterfaceIndex(1))->SetSendAdvert(false);

    ApplicationContainer radvdApps = radvdHelper.Install(r);
    radvdApps.Start(Seconds(1.0));
    radvdApps.Stop(Seconds(10.0));

    /* Create a Ping application to send ICMPv6 echo request from n0 to n1 via R */
    uint32_t packetSize = 1024;
    uint32_t maxPacketCount = 5;
    PingHelper ping(
        Ipv6Address("2001:2::200:ff:fe00:4")); /* should be n1 address after autoconfiguration */

    ping.SetAttribute("Count", UintegerValue(maxPacketCount));
    ping.SetAttribute("Size", UintegerValue(packetSize));
    ApplicationContainer apps = ping.Install(net1.Get(0));
    apps.Start(Seconds(2.0));
    apps.Stop(Seconds(7.0));

    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("radvd.tr"));
    csma.EnablePcapAll(std::string("radvd"), true);

    NS_LOG_INFO("Run Simulation.");
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    return 0;
}
