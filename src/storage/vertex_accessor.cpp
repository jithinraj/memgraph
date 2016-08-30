#include "storage/vertex_accessor.hpp"

#include "database/db.hpp"
#include "storage/vertex_record.hpp"
#include "storage/vertices.hpp"
#include "utils/iterator/iterator.hpp"

bool VertexAccessor::isolated() const
{
    return out_degree() == 0 && in_degree() == 0;
}

size_t VertexAccessor::out_degree() const
{
    return this->record->data.out.degree();
}

size_t VertexAccessor::in_degree() const
{
    return this->record->data.in.degree();
}

size_t VertexAccessor::degree() const { return in_degree() + out_degree(); }

bool VertexAccessor::add_label(const Label &label)
{
    // update vertex
    return this->record->data.labels.add(label);
}

bool VertexAccessor::remove_label(const Label &label)
{
    // update vertex
    return this->record->data.labels.remove(label);
}

bool VertexAccessor::has_label(const Label &label) const
{
    return this->record->data.labels.has(label);
}

const std::vector<label_ref_t> &VertexAccessor::labels() const
{
    return this->record->data.labels();
}

bool VertexAccessor::in_contains(VertexAccessor const &other) const
{
    return record->data.in.contains(other.vlist);
}

void VertexAccessor::remove() const
{
    RecordAccessor::remove();

    for (auto evr : record->data.out) {
        auto ea = EdgeAccessor(evr, db);
        ea.vlist->remove(db.trans);
        auto to_v = ea.to();
        to_v.fill();
        to_v.update().record->data.in.remove(ea.vlist);
    }
    for (auto evr : record->data.in) {
        auto ea = EdgeAccessor(evr, db);
        ea.vlist->remove(db.trans);
        auto from_v = ea.from();
        from_v.fill();
        from_v.update().record->data.out.remove(ea.vlist);
    }
}
